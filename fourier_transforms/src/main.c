#include "platform.h"
#include "raylib.h"
#define RAYGUI_SUPPORT_ICONS
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#define NUM_SAMPLES 1024

typedef struct state
{
    f32 Signal[NUM_SAMPLES];
    f32 FourierTransform[NUM_SAMPLES];
} state;

internal void 
SlowFourierTransform(f32 *TimeDomain, f32 *FreqDomain, i32 Size)
{
    // TODO: Make a optimized fast fourier transform.
    // 0Hz - 10Hz.
    for (i32 Ki = 0; Ki < Size; Ki++)
    {
        f32 K = ((f32)Ki/Size) * 10.0f;
        f32 SumX = 0;
        f32 SumY = 0;
        for (i32 N = 0; N < Size; N++)
        {
            f32 Sample = TimeDomain[N];
            f32 Ns = (f32)N/Size;
            f32 Theta = 2*PI*K*Ns;
            f32 Magnitude = Sample;
            f32 X = sinf(Theta) * Magnitude;
            f32 Y = cosf(Theta) * Magnitude;
            SumX += X;
            SumY += Y;
        }
        FreqDomain[Ki] = SumX;
    }
}

internal void 
Update(state *State)
{
    static i32 Freq1 = 4;
    static bool Freq1_Edit = false;
    static f32 Amp1 = 1;
    
    static i32 Freq2 = 0;
    static bool Freq2_Edit = false;
    static f32 Amp2 = 1;
    
    static i32 Freq3 = 0;
    static f32 Amp3 = 1;
    static bool Freq3_Edit = false;
    
    if (GuiSpinner((Rectangle){ 600, 15, 80, 30 }, NULL, &Freq1, 0, 10, Freq1_Edit)) 
        Freq1_Edit = !Freq1_Edit;
    if (GuiSpinner((Rectangle){ 700, 15, 80, 30 }, NULL, &Freq2, 0, 10, Freq2_Edit)) 
        Freq2_Edit = !Freq2_Edit;
    if (GuiSpinner((Rectangle){ 800, 15, 80, 30 }, NULL, &Freq3, 0, 10, Freq3_Edit)) 
        Freq3_Edit = !Freq3_Edit;
    
    for (i32 N = 0; N < NUM_SAMPLES; N++)
    {
        State->Signal[N] = 0;
        State->Signal[N] += Amp1 * sinf(2 * PI * Freq1 * ((f32)N / NUM_SAMPLES));
        State->Signal[N] += Amp2 * sinf(2 * PI * Freq2 * ((f32)N / NUM_SAMPLES));
        State->Signal[N] += Amp3 * sinf(2 * PI * Freq3 * ((f32)N / NUM_SAMPLES));
        State->Signal[N] /= 3;
    }
}

internal void
Draw(state *State)
{
    Color SignalCol = ColorAlpha(RED, 0.7f);
    Color BaselineCol = ColorAlpha(GRAY, 0.7f);
    Color CutCol = ColorAlpha(GREEN, 0.9f);
    Color Sum2DCol = ColorAlpha(YELLOW, 0.9f);
    f32 MouseX = (f32)GetMouseX();
    
    // Draw the signal
    i32 SignalBaselineY = 150;
    i32 SignalHeight = 50;
    DrawLine(0, SignalBaselineY, NUM_SAMPLES, SignalBaselineY, BaselineCol);
    for (i32 N = 0; N < NUM_SAMPLES; N++)
    {
        f32 Sample = State->Signal[N];
        DrawPixel(N, SignalBaselineY + Sample*SignalHeight, SignalCol);
    }
    
    // Cut Frequency
    f32 MaxCutFreq = 10.0f;
    f32 CutFreq = (MouseX/NUM_SAMPLES) * MaxCutFreq;
    
    // Draw Cuts
    i32 CutX = 0;
    i32 CutStride = max((NUM_SAMPLES/CutFreq), 1);
    f32 Summation[NUM_SAMPLES] = {0};
    while (CutStride > 0 && CutX < NUM_SAMPLES)
    {
        DrawLine(CutX, 
                 SignalBaselineY - SignalHeight, 
                 CutX, 
                 SignalBaselineY + SignalHeight, 
                 CutCol);
        // Draw overlap.
        for (i32 N = 0; (N < CutStride) && (N+CutX < NUM_SAMPLES); N++)
        {
            f32 Sample = State->Signal[N + CutX];
            DrawPixel(CutStride + N,
                      SignalBaselineY + Sample*SignalHeight,
                      SignalCol);
            Summation[N] += Sample;
        }
        CutX += CutStride;
    }
    
    // Draw Summation.
    for (i32 N = 0; N < min(CutStride, NUM_SAMPLES); N++)
    {
        f32 Sample = Summation[N];
        DrawPixel(CutStride + N,
                  SignalBaselineY + Sample*SignalHeight,
                  CutCol);
    }
    
    // Draw circular wrap.
    i32 CircleOriginX = 512;
    i32 CircleOriginY = 400;
    f32 SumX = 0;
    f32 SumY = 0;
    for (i32 N = 0; N < NUM_SAMPLES; N++)
    {
        f32 Sample = State->Signal[N];
        f32 Ns = (f32)N/NUM_SAMPLES;
        f32 Theta = 2*PI*CutFreq*Ns;
        f32 Magnitude = 200 * Sample;
        f32 X = sinf(Theta) * Magnitude;
        f32 Y = cosf(Theta) * Magnitude;
        
        SumX += X;
        SumY += Y;
        
        DrawPixel(X + CircleOriginX, Y + CircleOriginY, CutCol);
    }
    
    // Draw 2D summation.
    f32 Sum2DScale = 0.01f;
    i32 SumXFinal = (i32)(SumX*Sum2DScale) + CircleOriginX;
    i32 SumYFinal = (i32)(SumY*Sum2DScale) + CircleOriginY;
    DrawCircle(SumXFinal, 
               SumYFinal, 
               5, 
               Sum2DCol);
    DrawLine(CircleOriginX,
             CircleOriginY,
             SumXFinal,
             SumYFinal,
             Sum2DCol);
    
    SlowFourierTransform(&State->Signal[0], &State->FourierTransform[0], NUM_SAMPLES);
    
    // Draw the frequency domain
    for (i32 N = 0; N < NUM_SAMPLES; N++)
    {
        f32 F = State->FourierTransform[N];
        DrawPixel(N,
                  650 - (F * 1),
                  Sum2DCol);
    }
    
    DrawText(TextFormat("Cut Freq: %.2fHz", CutFreq),
             15,
             15,
             20,
             RAYWHITE);
}

i32 
main(i32 argc, char **argv)
{
    const i32 screen_width = NUM_SAMPLES;
    const i32 screen_height = 768;
    InitWindow(screen_width, screen_height, "Raylib");
    SetTargetFPS(60);
    InitAudioDevice();
    
    state *State = malloc(sizeof(state));
    for (i32 N = 0; N < NUM_SAMPLES; N++)
    {
        State->FourierTransform[N] = 0;
    }
    
    while(!WindowShouldClose())
    {
        Update(State);
        
        BeginDrawing();
        ClearBackground(BLACK);
        Draw(State);
        EndDrawing();
    }
    
    CloseAudioDevice();
    CloseWindow();
    return 0;
}