#include "platform.h"
#include "raylib.h"
#define RAYGUI_SUPPORT_ICONS
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#define NUM_SAMPLES 1024
typedef struct
{
    f32 Samples[NUM_SAMPLES];
    f32 FourierTransform[NUM_SAMPLES];
} state;

internal void
FourierTransform(f32 *TimeDomain, f32 *FreqDomain, usize Size)
{
    for (i32 Fn = 0; Fn < Size; Fn++)
    {
        f32 Theta = ((f32)Fn/Size) * 10.0f;
        f32 SumX = 0;
        f32 SumY = 0;
        for (i32 Sn = 0; Sn < Size; Sn++)
        {
            f32 Ns = (f32)Sn/Size;
            f32 W = (2*PI) * Theta;
            f32 Xf = -sinf(Ns*W) * TimeDomain[Sn];
            f32 Yf = -cosf(Ns*W) * TimeDomain[Sn];
            SumX += Xf;
            SumY += Yf;
        }
        f32 Sum = (SumX*SumX + SumY*SumY) * 0.005f;
        //f32 Sum = SumX*SumX * 0.001f;
        FreqDomain[Fn] = Sum;
    }
}

internal void
Draw(state *State)
{
    f32 SignalStartY = 728/3;
    Vector2 MousePos = GetMousePosition();
    f32 WrapFreq = (MousePos.x / NUM_SAMPLES) * 10;
    DrawLine(0, SignalStartY, 1024, SignalStartY, ColorAlpha(GRAY, 0.6f));
    
    Vector2 SignalWrapOrigin = {500, 500};
    Vector2 SumPoint = {0, 0};
    for (i32 N = 0; N < NUM_SAMPLES; N++)
    {
        f32 Sample = State->Samples[N];
        i32 Y = SignalStartY + (Sample * 50);
        DrawPixel(N, Y, ColorAlpha(RED, 0.7f));
        
        f32 Ns = (f32)N/NUM_SAMPLES;
        f32 W = (2*PI) * WrapFreq;
        f32 Xf = -sinf(Ns*W) * Sample;
        f32 Yf = -cosf(Ns*W) * Sample;
        SumPoint.x += Xf;
        SumPoint.y += Yf;
        f32 Mag = 100.0f;
        DrawPixel((Xf * Mag) + SignalWrapOrigin.x, 
                  (Yf * Mag) + SignalWrapOrigin.y, 
                  ColorAlpha(GREEN, 0.4f));
    }
    
    Vector2 SumPoint2 = {
        SumPoint.x + SignalWrapOrigin.x,
        SumPoint.y + SignalWrapOrigin.y
    };
    DrawLineV(SignalWrapOrigin, SumPoint2, YELLOW);
    DrawCircleV(SumPoint2, 4, YELLOW);
    
    f32 Stride = NUM_SAMPLES / max(WrapFreq, 1.0f);
    {
        f32 OverlapArr[NUM_SAMPLES] = {0};
        f32 X = 0;
        while(X < NUM_SAMPLES)
        {
            f32 Y = SignalStartY;
            DrawLine(X, Y-100, X, Y+100, GREEN);
            for (i32 N = 0; N < Stride; N++)
            {
                if (N + X >= NUM_SAMPLES) break;
                f32 Sample = State->Samples[N + (i32)X];
                DrawPixel(Stride+N, SignalStartY + (Sample*50), ColorAlpha(RED, 0.4f));
                OverlapArr[N] += Sample;
            }
            X += max(Stride, 1);
        }
        
        if (IsMouseButtonDown(0))
        {
            for (i32 N = 0; N < Stride; N++)
            {
                DrawPixel(Stride+N, SignalStartY + (OverlapArr[N]*50), ColorAlpha(GREEN, 0.6f));
            }
        }
    }
    
    {
        
#if 0
        for(i32 N = 0; N < Stride; N++)
        {
            f32 X = 0;
            if (N + X >= NUM_SAMPLES) break;
            f32 Sum = 0;
            f32 XCount = 0;
            while(X < NUM_SAMPLES)
            {
                f32 Sample = State->Samples[N + (i32)X];
                Sum += Sample;
                X += max(Stride, 1);
                XCount += 1;
                DrawPixel(N, SignalStartY + (Sample*50), RAYWHITE);
            }
            //Sum /= XCount+1;
        }
#endif
    }
    
    f32 DistSum = sqrt(SumPoint.x*SumPoint.x + SumPoint.y*SumPoint.y);
    DrawText(TextFormat("Wrap Freq: %.1fHz", WrapFreq), 15, 25, 20, GREEN);
    DrawText(TextFormat("Distance: %.1f", DistSum), 15, 45, 20, YELLOW);
    
    i32 Xf = (NUM_SAMPLES/10) * WrapFreq;
    f32 FourierTransformStartY = 730;
    for (i32 N = 0; N < NUM_SAMPLES; N++)
    {
        f32 Sample = State->FourierTransform[N];
        i32 Y = FourierTransformStartY - (Sample * 1);
        DrawPixel(N, Y, YELLOW);
    }
    DrawLine(Xf, 
             FourierTransformStartY-(State->FourierTransform[Xf]), 
             Xf, 
             FourierTransformStartY+20, 
             YELLOW);
}

internal void
Update(state *State)
{
    static i32 Freq1 = 6;
    static bool Freq1_Edit = false;
    static f32 Amp1 = 1;
    
    static i32 Freq2 = 6;
    static bool Freq2_Edit = false;
    static f32 Amp2 = 1;
    
    static i32 Freq3 = 6;
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
        State->Samples[N] = 0;
        State->Samples[N] += Amp1 * cosf(2 * PI * Freq1 * ((f32)N / NUM_SAMPLES));
        State->Samples[N] += Amp2 * sinf(2 * PI * Freq2 * ((f32)N / NUM_SAMPLES));
        State->Samples[N] += Amp3 * sinf(2 * PI * Freq3 * ((f32)N / NUM_SAMPLES));
        State->Samples[N] /= 3;
    }
    
    FourierTransform(&State->Samples[0], &State->FourierTransform[0], NUM_SAMPLES);
}

i32 
main(i32 argc, char **argv)
{
    const i32 screen_width = 1024;
    const i32 screen_height = 768;
    InitWindow(screen_width, screen_height, "Raylib");
    SetTargetFPS(60);
    InitAudioDevice();
    
    f32 Freq1 = 4;
    f32 Freq2 = 3;
    f32 Freq3 = 9;
    f32 Amp1 = 0.5f;
    f32 Amp2 = 1.25f;
    f32 Amp3 = 2.25f;
    state *State = malloc(sizeof(state));
    for (i32 N = 0; N < NUM_SAMPLES; N++)
    {
        State->Samples[N] = 0;
        State->Samples[N] += Amp1 * sinf(2 * PI * Freq1 * ((f32)N / NUM_SAMPLES));
        State->Samples[N] += Amp2 * sinf(2 * PI * Freq2 * ((f32)N / NUM_SAMPLES));
        State->Samples[N] += Amp3 * sinf(2 * PI * Freq3 * ((f32)N / NUM_SAMPLES));
        State->Samples[N] /= 3;
    }
    for (i32 N = 0; N < NUM_SAMPLES; N++)
    {
        State->FourierTransform[N] = 0;
    }
    
    // @mainloop
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