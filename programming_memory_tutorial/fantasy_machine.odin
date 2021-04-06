package main
import "raylib"
import "core:fmt"
import "core:mem"
import "core:strings"
import "core:math/linalg"
import "core:math/rand"

NUM_CELL_ROWS :: 6;
NUM_CELL_COLS :: 12;
NUM_CELLS :: NUM_CELL_ROWS*NUM_CELL_COLS;
NUM_INSTRUCTIONS :: 64;
NUM_SNAPSHOTS :: 256*16;
NUM_REGISTERS :: 4;
NUM_LAST_ACCESSED_CELLS :: 4;

cell_view_type :: enum
{
    Number,
    Character,
    Count
}

instruction_type :: enum
{
    Nop,
    Write,
    WriteRegion,
    Checkpoint,
    CopyCellCell,
    CopyCellIndirect,
    CopyIndirectCell,
    CopyIndirectIndirect,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Equal,
    Less,
    Great,
    Or,
    And,
    JumpIf,
    JumpNotIf,
    Swap,
    PrintNumbers,
    PrintCharacters,
    PrintText,
    Load,
    Halt
}

instruction :: struct
{
    Type : instruction_type,
    Idx0, Idx1, Idx2 : u8,
    Val0, Val1 : u8,
    Text0 : string
}

cell_data_snapshot :: struct
{
    CellData : ^u8,
    Output : ^i8,
    InstructionIdx : uint,
    FakeCycles : uint
}

jump_label :: struct
{
    Key : string,
    Value : i32
}

app_state :: struct
{
    CellData : [NUM_CELLS]u8,
    Snapshots : [NUM_SNAPSHOTS]cell_data_snapshot,
    Instructions : [NUM_INSTRUCTIONS]instruction,
    LastAccessedCells : [NUM_LAST_ACCESSED_CELLS]int,
    RegistersStartIdx : int,
    JumpLabels : map[string]uint,
    InstructionIdx : uint,
    InstructionCount : uint,
    SnapshotIdx : uint,
    IsHalted : bool,
    CellViewType : cell_view_type,
}

main :: proc()
{
    using raylib;
    SCREEN_WIDTH :: 1400;
    SCREEN_HEIGHT :: 768;
    TARGET_FPS :: 30;

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Fantasy Machine");
    defer CloseWindow();

    SetTargetFPS(TARGET_FPS);
    Click01Sound : Sound = LoadSound("click_01.wav");
    SetSoundVolume(Click01Sound, 0.01);
    Click02Sound : Sound = LoadSound("click_02.wav");
    SetSoundVolume(Click02Sound, 0.01);

    AppState : ^app_state = new(app_state);
    defer free(AppState);

    // Randomize initial cell data.
    for _, Idx in AppState.CellData
    {
        AppState.CellData[Idx] = cast(u8)rand.uint32();
    }

    for !WindowShouldClose()
    {
        UpdateLoop(AppState);
        DrawLoop(AppState);
    }
}

UpdateLoop :: proc(AppState : ^app_state)
{
    using raylib;
    KEY_CHANGE_VIEW : i32 : cast(i32)raylib.KeyboardKey.KEY_SPACE;
    KEY_NEXT_STEP : i32 : cast(i32)raylib.KeyboardKey.KEY_RIGHT;
    KEY_NEXT_FAST : i32 : cast(i32)raylib.KeyboardKey.KEY_DOWN;
    KEY_PREV_STEP : i32 : cast(i32)raylib.KeyboardKey.KEY_LEFT;
    KEY_PREV_FAST : i32 : cast(i32)raylib.KeyboardKey.KEY_UP;

    if IsKeyPressed(KEY_CHANGE_VIEW)
    {
        val := cast(i32)AppState.CellViewType;
        val += 1;
        if cast(cell_view_type)val == cell_view_type.Count
        {
            val = 0;
        }
        AppState.CellViewType = cast(cell_view_type)val;
    }

    if (IsKeyPressed(KEY_NEXT_STEP) || IsKeyDown(KEY_NEXT_FAST)) &&
       (AppState.InstructionIdx < AppState.InstructionCount) &&
       (!AppState.IsHalted)
    {

    }
    else if (IsKeyPressed(KEY_PREV_STEP) || IsKeyDown(KEY_PREV_FAST)) &&
            (AppState.InstructionIdx > 0)
    {
        for I in 0..<NUM_LAST_ACCESSED_CELLS
            AppState.LastAccessedCells[I] = -1;
    }
}

DrawLoop :: proc(AppState : ^app_state)
{
    using raylib;
    BeginDrawing();
    defer EndDrawing();
    ClearBackground(BLACK);

    // Common parameters.
    CELL_WIDTH :: 70;
    CELL_HEIGHT :: 90;
    ALL_CELLS_X :: 500;
    ALL_CELLS_Y :: 100;
    CELL_LINE_WIDTH :: 1;
    CELL_ADDRESS_LINE_LOCAL_Y :: 20;
    CELL_ADDRESS_FONT_SIZE :: 10;
    CELL_COLOR :: raylib.RAYWHITE;
    CELL_DATA_FONT_SIZE :: 30;
    ALL_CELLS_END_X :: ALL_CELLS_X + ((CELL_WIDTH-CELL_LINE_WIDTH) * NUM_CELL_COLS);
    ALL_CELLS_END_Y :: ALL_CELLS_Y + ((CELL_HEIGHT-CELL_LINE_WIDTH) * NUM_CELL_ROWS);
    PIXEL_RECT :: raylib.Rectangle{
        x = ALL_CELLS_END_X - (6*NUM_CELL_COLS),
        y = 50,
        width = 6,
        height = 6
    };

    // Draw clock.
    CLOCK_X :: 50;
    CLOCK_Y :: 50;
    CLOCK_FONT_SIZE :: 20;
    CLOCK_COLOR :: raylib.BLUE;
    ClockText : string = fmt.aprintf("CLOCK: %08d", AppState.SnapshotIdx);
    defer delete(ClockText);
    DrawString(ClockText, CLOCK_X, CLOCK_Y, CLOCK_FONT_SIZE, CLOCK_COLOR);

    // Draw pixel representation of memory.
    DrawRectangleLines(cast(i32)PIXEL_RECT.x-1, 
                       cast(i32)PIXEL_RECT.y-1.0, 
                       cast(i32)(NUM_CELL_COLS*PIXEL_RECT.width)+2.0, 
                       cast(i32)(NUM_CELL_ROWS*PIXEL_RECT.height)+2.0, 
                       BLUE);
    for CellVal, Idx in AppState.CellData
    {
        X, Y := XYFromIndex(Idx);
        col := Color{CellVal, CellVal, CellVal, 255};
        DrawRectangle(
            cast(i32)(PIXEL_RECT.x + (X * PIXEL_RECT.width)),
            cast(i32)(PIXEL_RECT.y + (Y * PIXEL_RECT.height)),
            cast(i32)PIXEL_RECT.width,
            cast(i32)PIXEL_RECT.height,
            col
        );
    }

    // Draw Output
    OutputText : string = fmt.aprintf("OUTPUT:\n%s", "TODO");
    defer delete(OutputText);
    OutputTextRect := Rectangle{
        x = ALL_CELLS_X,
        y = ALL_CELLS_END_Y + 10,
        width = 800,
        height = 400
    };
    DrawStringRec(GetFontDefault(), OutputText, OutputTextRect, 10, 2, true, BLUE);

    // Draw instructions.
    CODE_X :: 50;
    CODE_Y :: 100;
    CODE_WIDTH :: 360;
    CODE_FONT_SIZE :: 10;
    CODE_LINE_HEIGHT :: CODE_FONT_SIZE/2;
    CodeLineRect := Rectangle {
        x = CODE_X,
        y = CODE_Y + (f32(AppState.InstructionIdx) * (CODE_FONT_SIZE+CODE_LINE_HEIGHT)),
        width = CODE_WIDTH,
        height = CODE_FONT_SIZE
    };
    DrawRectangleRec(CodeLineRect, RED);
    DrawString("TODO", CODE_X, CODE_Y, CODE_FONT_SIZE, RAYWHITE);

    // Draw registers.
    REGISTER_START_X :: 500;
    REGISTER_SPACING_X :: 150;
    REGISTER_START_Y :: 50;
    REGISTER_FONT_SIZE :: 40;
    REGISTER_FONT_COLOR :: raylib.RAYWHITE;
    REGISTER_HIGHLIGHT_PADDING :: 5;
    RegisterTextWidth : f32 = cast(f32)MeasureText("A: 255", REGISTER_FONT_SIZE);
    RegisterHighlightRect := Rectangle{
        x = REGISTER_START_X + REGISTER_HIGHLIGHT_PADDING,
        y = REGISTER_START_Y + REGISTER_HIGHLIGHT_PADDING,
        width = RegisterTextWidth - (REGISTER_HIGHLIGHT_PADDING * 2),
        height = REGISTER_FONT_SIZE - (REGISTER_HIGHLIGHT_PADDING * 2)
    };
    for RegisterIdx in 0..<NUM_REGISTERS
    {
        for I in 0..<NUM_LAST_ACCESSED_CELLS
        {
            if AppState.LastAccessedCells[I] == (AppState.RegistersStartIdx + RegisterIdx)
            {
                Origin := Vector2{
                    x = cast(f32)(-REGISTER_SPACING_X * RegisterIdx),
                    y = 0
                };
                DrawRectanglePro(RegisterHighlightRect, Origin, 0, (I == 2) ? GREEN : RED);
            }
        }
        OutputText : string = fmt.aprintf("%c: %03d", 'A', 255);
        defer delete(OutputText);
        DrawString(OutputText, 
                   REGISTER_START_X + (REGISTER_SPACING_X * cast(i32)RegisterIdx),
                   REGISTER_START_Y,
                   REGISTER_FONT_SIZE,
                   REGISTER_FONT_COLOR);
    }

    // Draw memory cells.
    CELL_INDIRECT_HIGHLIGHT_COLOR :: raylib.BLUE;
    CELL_BORDER_COLOR :: raylib.RED;
    CELL_HIGHLIGHT_COLOR_0 :: raylib.RED;
    CELL_HIGHLIGHT_COLOR_1 :: raylib.GREEN;
    for CellVal, CellIdx in AppState.CellData
    {
        X, Y := XYFromIndex(CellIdx);
        CellRect := Rectangle{
            x = ALL_CELLS_X + (X * (CELL_WIDTH - CELL_LINE_WIDTH)),
            y = ALL_CELLS_Y + (Y * (CELL_HEIGHT - CELL_LINE_WIDTH)),
            width = CELL_WIDTH,
            height = CELL_HEIGHT
        };
        DrawRectangleLinesEx(CellRect, 1, CELL_BORDER_COLOR);
        FILL_PADDING :: 10;
        FillRect := Rectangle{
            x = CellRect.x + FILL_PADDING,
            y = CellRect.y + CELL_ADDRESS_LINE_LOCAL_Y + FILL_PADDING,
            width = CellRect.width - FILL_PADDING*2,
            height = CellRect.height - CELL_ADDRESS_LINE_LOCAL_Y - FILL_PADDING*2
        };
        for J in 0..<NUM_LAST_ACCESSED_CELLS
        {
            if J == AppState.LastAccessedCells[J] && J != 0
            {
                DrawRectangleRec(FillRect, 
                                (J == 2) ? CELL_HIGHLIGHT_COLOR_1 : CELL_HIGHLIGHT_COLOR_0);
            }
            if AppState.InstructionIdx >= AppState.InstructionCount
            {
                continue;
            }
            Instruction : instruction = AppState.Instructions[AppState.InstructionIdx];
            {
                using Instruction;
                IsIdx0Indirect : bool = (
                    Type == instruction_type.CopyIndirectCell || 
                    Type == instruction_type.CopyIndirectIndirect);
                IsIdx1Indirect : bool = (
                    Type == instruction_type.CopyCellIndirect ||
                    Type == instruction_type.CopyIndirectIndirect);
                IsIdx0RefThisCell : bool = (cast(u8)J == AppState.CellData[Idx0]);
                IsIdx1RefThisCell : bool = (cast(u8)J == AppState.CellData[Idx1]);
                IsIdx0ARegister : bool = (Idx0 >= cast(u8)AppState.RegistersStartIdx);
                IsIdx1ARegister : bool = (Idx1 >= cast(u8)AppState.RegistersStartIdx);
                IsIndirectRegister : bool = (
                    (IsIdx0Indirect && IsIdx0RefThisCell && IsIdx0ARegister) ||
                    (IsIdx1Indirect && IsIdx1RefThisCell && IsIdx1ARegister));
                IsIndirect : bool = (
                    (IsIdx0Indirect && IsIdx0RefThisCell) ||
                    (IsIdx1Indirect && IsIdx1RefThisCell));
                if IsIndirectRegister
                {
                    RegisterIdx := IsIdx0RefThisCell ? (Idx0 - NUM_CELLS) : (Idx1 - NUM_CELLS);
                    DrawRectangleRec(FillRect, CELL_INDIRECT_HIGHLIGHT_COLOR);
                    RegisterOrigin := Vector2{
                        x = REGISTER_SPACING_X * cast(f32)RegisterIdx,
                        y = RegisterHighlightRect.height
                    };
                    DrawLine(cast(i32)(RegisterHighlightRect.x + RegisterOrigin.x),
                             cast(i32)(RegisterHighlightRect.y + RegisterOrigin.y),
                             cast(i32)FillRect.x,
                             cast(i32)FillRect.y,
                             CELL_INDIRECT_HIGHLIGHT_COLOR);
                }
                else if IsIndirect
                {
                    SelectedIdx : u8 = (IsIdx0RefThisCell) ? Idx0 : Idx1;
                    X, Y := XYFromIndex(cast(int)SelectedIdx);
                    Xp : f32 = ALL_CELLS_X + X * (CELL_WIDTH - CELL_LINE_WIDTH);
                    Yp : f32 = ALL_CELLS_Y + Y * (CELL_HEIGHT - CELL_LINE_WIDTH);
                    DrawRectangleRec(FillRect, CELL_INDIRECT_HIGHLIGHT_COLOR);
                    DrawLine(cast(i32)(Xp + (CELL_WIDTH*0.5)),
                             cast(i32)(Yp + (CELL_HEIGHT*0.5)),
                             cast(i32)FillRect.x,
                             cast(i32)FillRect.y,
                             CELL_INDIRECT_HIGHLIGHT_COLOR);
                }
            }
        }
        LineLeft := Vector2{
            x = CellRect.x,
            y = CellRect.y + CELL_ADDRESS_LINE_LOCAL_Y
        };
        LineRight := Vector2{
            x = CellRect.x + CellRect.width,
            y = CellRect.y + CELL_ADDRESS_LINE_LOCAL_Y
        };
        DrawLineEx(LineLeft, LineRight, 1, CELL_COLOR);
        AddressText : string = fmt.aprintf("%02d", CellIdx);
        defer delete(AddressText);
        AddressTextWidth : f32 = cast(f32)MeasureString(AddressText, CELL_ADDRESS_FONT_SIZE);
        HalfCellWidth : f32 = CellRect.width * 0.5;
        HalfCellHeight : f32 = CellRect.height * 0.5;
        DrawString(AddressText, 
                cast(i32)(CellRect.x + HalfCellWidth - (AddressTextWidth*0.5)),
                cast(i32)(CellRect.y + (CELL_ADDRESS_LINE_LOCAL_Y*0.5) - (CELL_ADDRESS_FONT_SIZE*0.5)),
                CELL_ADDRESS_FONT_SIZE,
                CELL_COLOR);

        DataX : f32 = CellRect.x + HalfCellWidth;
        DataY : f32 = CellRect.y + 
                    CELL_ADDRESS_LINE_LOCAL_Y + 
                    ((CellRect.height - CELL_ADDRESS_LINE_LOCAL_Y) * 0.5);
        if CellIdx == 0
        {
            DrawString("!", cast(i32)DataX, cast(i32)DataY - 15, 30, RED);
        }
        else
        {
            DataText : string = ---;
            #partial switch AppState.CellViewType
            {
                case .Number:
                    DataText = fmt.aprintf("%d", AppState.CellData[CellIdx]);
                case .Character:
                    DataText = fmt.aprintf("%c", AppState.CellData[CellIdx]);
            }

            DataTextWidth : f32 = cast(f32)MeasureString(DataText, CELL_DATA_FONT_SIZE);
            DrawString(DataText, 
                    cast(i32)(DataX - (DataTextWidth*0.5)),
                    cast(i32)(DataY - (CELL_DATA_FONT_SIZE*0.5)),
                    CELL_DATA_FONT_SIZE,
                    CELL_COLOR);
            delete(DataText);
        }
    }
}

XYFromIndex :: proc(Idx : int) -> (f32, f32)
{
    return cast(f32)(Idx % NUM_CELL_COLS), cast(f32)(Idx / NUM_CELL_COLS);
}