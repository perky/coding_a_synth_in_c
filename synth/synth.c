#include <stdio.h> // standard input and ouput.
#include <math.h>
#include <stdlib.h>
#include "raylib.h"
#define RAYGUI_SUPPORT_ICONS
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "midi.h"

#define SYNTH_SLOW 1 // run assertions.
#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768
#define TARGET_FPS 60
#define SAMPLE_RATE 44100
#define SAMPLE_DURATION (1.0f / SAMPLE_RATE)
#define STREAM_BUFFER_SIZE 1024
#define MAX_OSCILLATORS 32
#define MAX_UI_OSCILLATORS 32
#define UI_PANEL_WIDTH 350
#define BASE_NOTE_FREQ 440

global MidiKeyArray midi_keys = {0};
global MidiHandle midi_input_handle = 0;

typedef f32 (*WaveShapeFn)(const f32 phase_ratio, 
                           const f32 phase_dt, 
                           const f32 shape_param);

#define WAVE_SHAPE_OPTIONS "None;Sine;Sawtooth;Square;Triangle;Rounded Square"
typedef enum WaveShape {
    WaveShape_NONE = 0,
    WaveShape_SINE = 1,
    WaveShape_SAWTOOTH = 2,
    WaveShape_SQUARE = 3,
    WaveShape_TRIANGLE = 4,
    WaveShape_ROUNDEDSQUARE = 5,
    WaveShape_COUNT
} WaveShape;

typedef struct UiOscillator {
    f32 freq;
    f32 amplitude_ratio;
    f32 shape_parameter_0;
    WaveShape shape;
    bool is_dropdown_open;
    Rectangle shape_dropdown_rect;
    u8 modulation_state; // 0 = no modulation.
} UiOscillator;

typedef struct Oscillator {
    f32 phase_ratio;
    f32 phase_dt;
    f32 freq;
    f32 amplitude_ratio;
    f32 shape_parameter_0;
    u16 ui_id;
    bool is_modulator;
    f32 buffer[STREAM_BUFFER_SIZE];
} Oscillator;

typedef struct OscillatorArray {
    Oscillator osc[MAX_OSCILLATORS];
    usize count;
    WaveShapeFn wave_shape_fn;
} OscillatorArray;

typedef struct ModulationPair {
    Oscillator *modulator;
    Oscillator *carrier;
    u16 modulation_id;
    f32 modulation_ratio;
} ModulationPair;

typedef struct ModulationPairArray {
    ModulationPair data[MAX_OSCILLATORS];
    usize count;
} ModulationPairArray;

typedef struct Synth {
    OscillatorArray oscillator_groups[WaveShape_COUNT-1];
    usize oscillator_groups_count;
    
    f32 *signal;
    usize signal_count;
    f32 audio_frame_duration;
    
    UiOscillator ui_oscillator[MAX_UI_OSCILLATORS];
    usize ui_oscillator_count;
    
    ModulationPairArray modulation_pairs;
} Synth;

internal f32
FrequencyFromSemitone(f32 semitone)
{
    return powf(2.f, semitone/12.f) * BASE_NOTE_FREQ;
}

internal f32
SemitoneFromFrequency(f32 freq)
{
    return 12.f * Log2f(freq / BASE_NOTE_FREQ);
}

internal Oscillator*
NextOscillator(OscillatorArray* osc_array)
{
    Assert(osc_array->count < MAX_OSCILLATORS);
    return osc_array->osc + (osc_array->count++);
}

internal void
ClearOscillatorArray(OscillatorArray* osc_array)
{
    osc_array->count = 0;
}

internal void 
UpdatePhase(f32 *phase_ratio, f32 *phase_dt, f32 freq, f32 freq_mod)
{
    *phase_dt = ((freq + freq_mod) * SAMPLE_DURATION);
    *phase_ratio = *phase_ratio + *phase_dt;
    if (*phase_ratio < 0.0f)
        *phase_ratio += 1.0f;
    /
        if (*phase_ratio >= 1.0f)
        *phase_ratio -= 1.0f;
}

internal void 
UpdatePhaseInOsc(Oscillator *osc)
{
    osc->phase_dt = ((osc->freq) * SAMPLE_DURATION);
    osc->phase_ratio += osc->phase_dt;
    if (osc->phase_ratio < 0.0f)
        osc->phase_ratio += 1.0f;
    if (osc->phase_ratio >= 1.0f)
        osc->phase_ratio -= 1.0f;
}

internal void 
ZeroSignal(f32* signal)
{
    for(usize t = 0; t < STREAM_BUFFER_SIZE; t++)
    {
        signal[t] = 0.0f;
    }
}

// @shapefn
internal f32 
BandlimitedRipple(f32 phase_ratio, f32 phase_dt)
{
    if (phase_ratio < phase_dt)
    {
        phase_ratio /= phase_dt;
        return (phase_ratio+phase_ratio) - (phase_ratio*phase_ratio) - 1.0f;
    }
    else if (phase_ratio > 1.0f - phase_dt) 
    {
        phase_ratio = (phase_ratio - 1.0f) / phase_dt;
        return (phase_ratio*phase_ratio) + (phase_ratio+phase_ratio) + 1.0f;
    }
    else return 0.0f;
}


// NOTE(luke): Remove this before next episode
inline f32
SineFast(f32 x)
{
    //x = fmodf(x, 2*PI);
    f32 a = 0.083f;
    f32 a2 = 9.424778f * a;
    f32 a3 = 19.739209f * a;
    f32 xx = x*x;
    f32 xxx = xx*x;
    return (a * xxx) - (a2 * xx) + (a3 * x);
}

// @shapefn
internal f32
SineShape(const f32 phase_ratio, const f32 phase_dt, const f32 shape_param)
{
    return SineFast(2.f * PI * phase_ratio);
}

// @shapefn
internal f32 
SawtoothShape(const f32 phase_ratio, const f32 phase_dt, const f32 shape_param)
{
    f32 sample = (phase_ratio * 2.0f) - 1.0f;
    sample -= BandlimitedRipple(phase_ratio, phase_dt);
    return sample;
}

// @shapefn
internal f32 
TriangleShape(const f32 phase_ratio, const f32 phase_dt, const f32 shape_param)
{
    // TODO: Make this band-limited.
    if (phase_ratio < 0.5f)
        return (phase_ratio * 4.0f) - 1.0f;
    else
        return (phase_ratio * -4.0f) + 3.0f;
}

// @shapefn
internal f32 
SquareShape(const f32 phase_ratio, const f32 phase_dt, const f32 shape_param)
{
    f32 duty_cycle = shape_param;
    f32 sample = (phase_ratio < duty_cycle) ? 1.0f : -1.0f;
    sample += BandlimitedRipple(phase_ratio, phase_dt);
    sample -= BandlimitedRipple(fmodf(phase_ratio + (1.f - duty_cycle), 1.0f), phase_dt);
    return sample;
}

// @shapefn
internal f32
RoundedSquareShape(const f32 phase_ratio, const f32 phase_dt, const f32 shape_param)
{
    f32 s = (shape_param * 8.f) + 2.f;
    f32 base = (f32)fabs(s);
    f32 power = s * sinf(phase_ratio * PI * 2);
    f32 denominator = powf(base, power) + 1.f;
    f32 sample = (2.f / denominator) - 1.f;
    return sample;
}

internal void 
UpdateOscArray(OscillatorArray *osc_array, ModulationPairArray *mod_array)
{
    for (i32 i = 0; i < osc_array->count; i++)
    {
        Oscillator *osc = &(osc_array->osc[i]);
        if (osc->freq > (SAMPLE_RATE/2) || osc->freq < -(SAMPLE_RATE/2)) continue;
        
        ModulationPair *modulation = 0;
        for(usize mod_i = 0;
            mod_i < mod_array->count;
            mod_i++)
        {
            if (mod_array->data[mod_i].carrier == osc)
            {
                modulation = &mod_array->data[mod_i];
                break;
            }
        }
        
        
        for(usize t = 0; t < STREAM_BUFFER_SIZE; t++)
        {
            f32 freq_mod = 0.0f;
            if (modulation)
            {
                freq_mod = modulation->modulator->buffer[t] * modulation->modulation_ratio;
            }
            
            UpdatePhase(&osc->phase_ratio, 
                        &osc->phase_dt, 
                        osc->freq, 
                        freq_mod);
            
            f32 sample = osc_array->wave_shape_fn(osc->phase_ratio,
                                                  osc->phase_dt,
                                                  osc->shape_parameter_0);
            sample *= osc->amplitude_ratio;
            osc->buffer[t] = sample;
        }
    }
}

internal void
AccumulateOscillatorsIntoSignal(Synth *synth)
{
    for (usize i = 0; 
         i < synth->oscillator_groups_count; 
         i++)
    {
        OscillatorArray *osc_array = &synth->oscillator_groups[i];
        for (usize osc_i = 0; 
             osc_i < osc_array->count; 
             osc_i++)
        {
            Oscillator *osc = &(osc_array->osc[osc_i]);
            
            if (osc->is_modulator) continue;
            
            for (usize t = 0;
                 t < STREAM_BUFFER_SIZE;
                 t++)
            {
                synth->signal[t] += osc->buffer[t];
            }
        }
    }
}

// @mainloop
internal void 
HandleAudioStream(AudioStream stream, Synth* synth)
{
    f32 audio_frame_duration = 0.0f;
    if (IsAudioStreamProcessed(stream))
    {                                                            
        const f32 audio_frame_start_time = GetTime();
        ZeroSignal(synth->signal);
        for (usize i = 0;
             i < synth->oscillator_groups_count;
             i++)
        {
            OscillatorArray *osc_array = &synth->oscillator_groups[i];
            UpdateOscArray(osc_array, &synth->modulation_pairs);
        }
        
        AccumulateOscillatorsIntoSignal(synth);
        UpdateAudioStream(stream, synth->signal, synth->signal_count);
        synth->audio_frame_duration = GetTime() - audio_frame_start_time;
    }
}

// @drawfn
internal void
DrawSignal(Synth *synth)
{
    // Drawing the signal.
    {
        i32 zero_crossing_index = 0;
        for (i32 i = 1; 
             i < synth->signal_count; 
             i++)
        {
            if (synth->signal[i] >= 0.0f && synth->signal[i-1] < 0.0f) // zero-crossing
            {
                zero_crossing_index = i;
                break;
            }
        }
        
        Vector2 signal_points[STREAM_BUFFER_SIZE];
        const f32 screen_vertical_midpoint = (SCREEN_HEIGHT/2);
        for (i32 point_idx = 0; 
             point_idx < synth->signal_count; 
             point_idx++)
        {
            const i32 signal_idx = (point_idx + zero_crossing_index) % STREAM_BUFFER_SIZE;
            signal_points[point_idx].x = (f32)point_idx + UI_PANEL_WIDTH;
            signal_points[point_idx].y = screen_vertical_midpoint + (i32)(synth->signal[signal_idx] * 300);
        }
        DrawLineStrip(signal_points, STREAM_BUFFER_SIZE - zero_crossing_index, RED);
    }
}


// @drawfn
internal void 
DrawUi(Synth *synth)
{
    const i32 panel_x_start = 0;
    const i32 panel_y_start = 0;
    const i32 panel_width = UI_PANEL_WIDTH;
    const i32 panel_height = SCREEN_WIDTH;
    
    bool is_shape_dropdown_open = false;
    i32 shape_index = 0;
    
    GuiGrid((Rectangle){0,0,SCREEN_WIDTH,SCREEN_HEIGHT}, SCREEN_HEIGHT/8, 2);
    GuiPanel((Rectangle){
                 panel_x_start,
                 panel_y_start,
                 panel_width,
                 panel_height
             });
    
    bool click_add_oscillator = GuiButton((Rectangle){
                                              panel_x_start + 10,
                                              panel_y_start + 10,
                                              panel_width - 20,
                                              25
                                          }, "Add Oscillator");
    if (click_add_oscillator)
    {
        synth->ui_oscillator_count += 1;
        // Set defaults:
        UiOscillator *ui_osc = synth->ui_oscillator + (synth->ui_oscillator_count - 1);
        ui_osc->shape = WaveShape_SINE;
        ui_osc->freq = BASE_NOTE_FREQ;
        ui_osc->amplitude_ratio = 0.1f;
        ui_osc->shape_parameter_0 = 0.5f;
    }
    
    f32 panel_y_offset = 0;
    for (i32 ui_osc_i = 0; ui_osc_i < synth->ui_oscillator_count; ui_osc_i++)
    {
        UiOscillator* ui_osc = &synth->ui_oscillator[ui_osc_i];
        const bool has_shape_param = (ui_osc->shape == WaveShape_SQUARE || ui_osc->shape == WaveShape_ROUNDEDSQUARE);
        
        const i32 osc_panel_width = panel_width - 20;
        const i32 osc_panel_height = has_shape_param ? 130 : 100;
        const i32 osc_panel_x = panel_x_start + 10;
        const i32 osc_panel_y = panel_y_start + 50 + panel_y_offset;
        panel_y_offset += osc_panel_height + 5;
        GuiPanel((Rectangle){
                     osc_panel_x,
                     osc_panel_y,
                     osc_panel_width,
                     osc_panel_height
                 });
        
        const f32 slider_padding = 50.f;
        const f32 el_spacing = 5.f;
        Rectangle el_rect = {
            .x = osc_panel_x + slider_padding + 30,
            .y = osc_panel_y + 10,
            .width = osc_panel_width - (slider_padding * 2),
            .height = 25
        };
        
        // Frequency slider
        u8 freq_slider_label[16];
        sprintf(freq_slider_label, "%.1fHz", ui_osc->freq);
        f32 log_freq = log10f(ui_osc->freq);
        log_freq = GuiSlider(el_rect, 
                             freq_slider_label, 
                             "", 
                             log_freq, 
                             -1.0f, 
                             log10f((f32)(SAMPLE_RATE/2))
                             );
        ui_osc->freq = powf(10.f, log_freq);
        el_rect.y += el_rect.height + el_spacing;
        
        // Amplitude slider
        f32 decibels = (20.f * log10f(ui_osc->amplitude_ratio));
        u8 amp_slider_label[32];
        sprintf(amp_slider_label, "%.1f dB", decibels);
        decibels = GuiSlider(el_rect, 
                             amp_slider_label, 
                             "", 
                             decibels, 
                             -60.0f, 
                             0.0f
                             );
        ui_osc->amplitude_ratio = powf(10.f, decibels * (1.f/20.f));
        el_rect.y += el_rect.height + el_spacing;
        
        // Shape parameter slider
        if (has_shape_param)
        {
            f32 shape_param = ui_osc->shape_parameter_0;
            u8 shape_param_label[32];
            sprintf(shape_param_label, "%.1f", shape_param);
            shape_param = GuiSlider(el_rect,
                                    shape_param_label,
                                    "",
                                    shape_param,
                                    0.f,
                                    1.f
                                    );
            ui_osc->shape_parameter_0 = shape_param;
            el_rect.y += el_rect.height + el_spacing;
        }
        
        // Defer shape drop-down box.
        ui_osc->shape_dropdown_rect = el_rect;
        el_rect.y += el_rect.height + el_spacing;
        
        Rectangle delete_button_rect = el_rect;
        delete_button_rect.x = osc_panel_x + 5;
        delete_button_rect.y -= el_rect.height + el_spacing;
        delete_button_rect.width = 30;
        bool is_delete_button_pressed = GuiButton(delete_button_rect, "X");
        if (is_delete_button_pressed)
        {
            memmove(
                    synth->ui_oscillator + ui_osc_i, 
                    synth->ui_oscillator + ui_osc_i + 1, 
                    (synth->ui_oscillator_count - ui_osc_i) * sizeof(UiOscillator)
                    );
            synth->ui_oscillator_count -= 1;
        }
        
        Rectangle modulation_button_rect = delete_button_rect;
        modulation_button_rect.x += 40;
        char *modulation_button_text = (ui_osc->modulation_state == 0) ? "N/A" : TextFormat("%d", ui_osc->modulation_state);
        bool modulation_button_pressed = GuiButton(modulation_button_rect, modulation_button_text);
        if (modulation_button_pressed)
        {
            ui_osc->modulation_state += 1;
            if (ui_osc->modulation_state == 8)
            {
                ui_osc->modulation_state = 0;
            }
        }
    }
    
    for (i32 ui_osc_i = 0; 
         ui_osc_i < synth->ui_oscillator_count; 
         ui_osc_i += 1)
    {
        UiOscillator* ui_osc = &synth->ui_oscillator[ui_osc_i];
        // Shape select
        i32 shape_index = (i32)(ui_osc->shape);
        bool is_dropdown_click = GuiDropdownBox(ui_osc->shape_dropdown_rect, 
                                                WAVE_SHAPE_OPTIONS, 
                                                &shape_index, 
                                                ui_osc->is_dropdown_open
                                                );
        ui_osc->shape = (WaveShape)(shape_index);
        if (is_dropdown_click)
        {
            ui_osc->is_dropdown_open = !ui_osc->is_dropdown_open;
        }
        if (ui_osc->is_dropdown_open) break;
    }
}

internal void
ApplyUiState(Synth *synth)
{
    // Reset synth
    // TODO(luke): make this something we can iterate.
    for (usize i = 0;
         i < synth->oscillator_groups_count;
         i++)
    {
        ClearOscillatorArray(&synth->oscillator_groups[i]);
    }
    synth->modulation_pairs.count = 0;
    
    for (i32 ui_osc_i = 0; 
         ui_osc_i < synth->ui_oscillator_count; 
         ui_osc_i++)
    {
        UiOscillator ui_osc = synth->ui_oscillator[ui_osc_i];
        
        for(i32 note_idx = midi_keys.count-1;
            note_idx >= 0;
            note_idx--)
        {
            MidiKey midi_key = midi_keys.data[note_idx];
            if (!midi_key.is_on) continue;
            
            Oscillator *osc = NULL;
            if (ui_osc.shape > 0 && ui_osc.shape < WaveShape_COUNT)
            {
                osc = NextOscillator(&synth->oscillator_groups[ui_osc.shape-1]);
            }
            
            if (osc != NULL)
            {
                osc->ui_id = ui_osc_i;
                f32 ui_semitone = SemitoneFromFrequency(ui_osc.freq);
                f32 midi_semitone = (f32)(midi_key.note - BASE_MIDI_NOTE);
                f32 osc_freq = FrequencyFromSemitone(ui_semitone + midi_semitone);
                osc->freq = osc_freq;
                osc->amplitude_ratio = ui_osc.amplitude_ratio;
                osc->shape_parameter_0 = ui_osc.shape_parameter_0;
                osc->is_modulator = false;
                
                if (ui_osc.modulation_state > 0 && (ui_osc.modulation_state-1) < synth->ui_oscillator_count)
                {
                    ModulationPair *mod_pair = synth->modulation_pairs.data + synth->modulation_pairs.count++;
                    mod_pair->modulator = 0;
                    mod_pair->carrier = osc;
                    mod_pair->modulation_id = ui_osc.modulation_state - 1;
                    mod_pair->modulation_ratio = 100.0f;
                }
            }
        }
    }
    
    for(usize mod_i = 0;
        mod_i < synth->modulation_pairs.count;
        mod_i++)
    {
        ModulationPair *mod_pair = &synth->modulation_pairs.data[mod_i];
        u16 shape_id = (u16)synth->ui_oscillator[mod_pair->modulation_id].shape;
        OscillatorArray *osc_array = &synth->oscillator_groups[shape_id-1];
        // TODO(luke): modulators (LFOs) should not be per-note.
        for (usize osc_i = 0;
             osc_i < osc_array->count;
             osc_i++)
        {
            Oscillator *osc = &osc_array->osc[osc_i];
            if (osc->ui_id == mod_pair->modulation_id)
            {
                if (mod_pair->modulator == 0)
                    mod_pair->modulator = osc;
                osc->is_modulator = true;
            }
        }
    }
}

i32 
main(i32 argc, char **argv)
{
    const i32 screen_width = 1024;
    const i32 screen_height = 768;
    InitWindow(screen_width, screen_height, "Synth");
    SetTargetFPS(TARGET_FPS);
    InitAudioDevice();
    midi_keys.count = ArrayCount(midi_keys.data);
    midi_input_handle = SynthMidiInit(0, &midi_keys);
    GuiLoadStyle(".\\raygui\\styles\\jungle\\jungle.rgs");
    
    u32 sample_rate = SAMPLE_RATE;
    SetAudioStreamBufferSizeDefault(STREAM_BUFFER_SIZE);
    AudioStream synth_stream = InitAudioStream(sample_rate, 
                                               sizeof(f32) * 8, 
                                               1);
    SetAudioStreamVolume(synth_stream, 0.01f);
    PlayAudioStream(synth_stream);
    
    ModulationPair modulation_pairs[256] = {0};
    f32 signal[STREAM_BUFFER_SIZE] = {0};
    
    printf("Oscillator size: %lld\n", sizeof(Oscillator));
    printf("UiOscillator size: %lld\n", sizeof(UiOscillator));
    printf("OscillatorArray size: %lld\n", sizeof(OscillatorArray));
    printf("Synth size: %lld\n", sizeof(Synth));
    
    Synth *synth = (Synth *)malloc(sizeof(Synth));
    synth->oscillator_groups_count = ArrayCount(synth->oscillator_groups);
    synth->signal = signal;
    synth->signal_count = ArrayCount(signal);
    
    synth->oscillator_groups[WaveShape_SINE-1].count = 0;
    synth->oscillator_groups[WaveShape_SINE-1].wave_shape_fn = SineShape;
    synth->oscillator_groups[WaveShape_SAWTOOTH-1].count = 0;
    synth->oscillator_groups[WaveShape_SAWTOOTH-1].wave_shape_fn = SawtoothShape;
    synth->oscillator_groups[WaveShape_TRIANGLE-1].count = 0;
    synth->oscillator_groups[WaveShape_TRIANGLE-1].wave_shape_fn = TriangleShape;
    synth->oscillator_groups[WaveShape_SQUARE-1].count = 0;
    synth->oscillator_groups[WaveShape_SQUARE-1].wave_shape_fn = SquareShape;
    synth->oscillator_groups[WaveShape_ROUNDEDSQUARE-1].count = 0;
    synth->oscillator_groups[WaveShape_ROUNDEDSQUARE-1].wave_shape_fn = RoundedSquareShape;
    
    synth->modulation_pairs.count = 0;
    
#if 0
    synth->modulation_pairs.count = 1;
    synth->modulation_pairs.data[0].modulator = &synth->oscillator_groups[WaveShape_SINE-1].osc[0];
    synth->modulation_pairs.data[0].carrier = &synth->oscillator_groups[WaveShape_SINE-1].osc[1];
    synth->modulation_pairs.data[0].modulation_ratio = 100.0f;
#endif
    
    // @mainloop
    while(!WindowShouldClose())
    {
        HandleAudioStream(synth_stream, synth);
        BeginDrawing();
        ClearBackground(BLACK);
        DrawUi(synth);
        ApplyUiState(synth);
        DrawSignal(synth);
        
        const f32 total_frame_duration = GetFrameTime();
        DrawText(FormatText("Frame time: %.3f%%, Audio budget: %.3f%%", 
                            (100.0f / (total_frame_duration * TARGET_FPS)), 
                            100.0f / ((1.0f / synth->audio_frame_duration) / ((f32)SAMPLE_RATE/STREAM_BUFFER_SIZE))),
                 UI_PANEL_WIDTH + 10, 10,
                 20,
                 RED);
        DrawText(FormatText("Fundemental Freq: %.1f", 
                            synth->oscillator_groups[0].osc[0].freq),
                 UI_PANEL_WIDTH + 10, 30,
                 20,
                 RED);
        EndDrawing();
    }
    
    if (midi_input_handle)
    {
        SynthMidiStop(midi_input_handle);
    }
    CloseAudioStream(synth_stream);
    CloseAudioDevice();
    CloseWindow();
    
    return 0;
}