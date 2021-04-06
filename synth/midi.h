#include <stdio.h>
#include "minimal_windows.h"
#include "synth_platform.h"

typedef MIDIINCAPS MidiDeviceInfo;
typedef HMIDIIN MidiHandle;

#define KEY_ON 144
#define KEY_OFF 128
#define BASE_MIDI_NOTE 69 // A4
#define MAX_MIDI_VELOCITY 127.f
#define POLYPHONIC_COUNT 16

// @midi
typedef union MidiMessage
{
    u32 message;
    u8 data[4];
} MidiMessage;

// @midi
typedef struct MidiKey
{
    u8 is_on;
    u8 note;
    f64 velocity_ratio;
} MidiKey;

// @midi
typedef struct MidiKeyArray
{
    MidiKey data[POLYPHONIC_COUNT];
    u32 count;
} MidiKeyArray;

// @midi
internal void CALLBACK 
SynthMidiHandler(MidiHandle midi, u32 msg_type, u32 *user_data, u32 param1, u32 param2)
{
    switch(msg_type)
    {
        case MIM_DATA: {
            MidiKeyArray *keys = (MidiKeyArray*)user_data;
            MidiMessage msg = {param1};
            u8 midi_event = msg.data[0];
            u8 midi_note = msg.data[1];
            u8 midi_velocity = msg.data[2];
            u32 midi_timestamp = param2;
            
            if (midi_event == KEY_ON)
            {
                MidiKey *key = 0;
                for (u32 i = 0; i < keys->count; i++)
                {
                    if (keys->data[i].is_on == 0)
                    {
                        key = keys->data + i;
                        break;
                    }
                }
                if (key)
                {
                    key->is_on = 1;
                    key->note = midi_note;
                    key->velocity_ratio = (f32)midi_velocity / MAX_MIDI_VELOCITY;
                }
            }
            else if (midi_event == KEY_OFF)
            {
                MidiKey *key = 0;
                for (u32 i = 0; i < keys->count; i++)
                {
                    if (keys->data[i].is_on && keys->data[i].note == midi_note)
                    {
                        keys->data[i].is_on = false;
                        break;
                    }
                }
            }
            break;
        }
    }
}

MidiHandle
SynthMidiInit(u32 selected_device_id, MidiKeyArray *keys)
{
    u32 num_midi_devices = midiInGetNumDevs();
    for(u32 device_id = 0; device_id < num_midi_devices; device_id++)
    {
        MidiDeviceInfo device_info;
        u32 result = midiInGetDevCaps(device_id,
                                      &device_info,
                                      sizeof(MidiDeviceInfo));
        if (result != MMSYSERR_NOERROR)
        {
            printf("Could not get MIDI device info\n");
            return 0;
        }
        printf("MIDI device: %s\n", device_info.szPname);
    }
    
    MidiHandle midi;
    u32 result = midiInOpen(&midi,
                            selected_device_id,
                            (DWORD_PTR)SynthMidiHandler,
                            (DWORD_PTR)keys,
                            CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR)
    {
        printf("Could not open MIDI device %d.\n", selected_device_id);
        return 0;
    }
    
    result = midiInStart(midi);
    if (result != MMSYSERR_NOERROR)
    {
        printf("Could not start MIDI device %d.\n", selected_device_id);
        return 0;
    }
    
    return midi;
}

void 
SynthMidiStop(MidiHandle midi)
{
    u32 result = midiInStop(midi);
    if (result != MMSYSERR_NOERROR)
    {
        printf("Could not stop MIDI device.\n");
        return;
    }
    
    result = midiInClose(midi);
    if (result != MMSYSERR_NOERROR)
    {
        printf("Could not close MIDI device.\n");
        return;
    }
}
