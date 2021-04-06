#include <stdio.h> // standard input and ouput.
#include <string.h>
#include <math.h>
#include "raylib.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define CODE_FONT_SIZE 10
#define SCREEN_WIDTH 1400
#define SCREEN_HEIGHT 768
#define MAX_INSTRUCTIONS 64
#define MAX_SNAPSHOTS 256*16
#define MAX_OUTPUT_SIZE 256*16
#define MAX_LAST_ACCESSED_CELLS 4

typedef enum {
    CELL_DATA_NUMBER,
    CELL_DATA_CHARACTER
} CellDataType;

typedef enum {
    INS_NOP,
    INS_WRITE,
    INS_WRITE_REGION,
    INS_CHECKPOINT,
    INS_COPY_CELL_CELL,
    INS_COPY_CELL_INDIRECT,
    INS_COPY_INDIRECT_CELL,
    INS_COPY_INDIRECT_INDIRECT,
    INS_ADD,
    INS_SUB,
    INS_MUL,
    INS_DIV,
    INS_MOD,
    INS_EQUAL,
    INS_LESS,
    INS_GREAT,
    INS_OR,
    INS_AND,
    INS_JUMP_IF,
    INS_JUMP_NOT_IF,
    INS_SWAP,
    INS_PRINT_NUMBERS,
    INS_PRINT_CHARACTERS,
    INS_PRINT_TEXT,
    INS_LOAD,
    INS_HALT
} InstructionType;

typedef struct {
    InstructionType type;
    int idx0;
    int idx1;
    int idx2;
    unsigned char val0;
    unsigned char val1;
    const char* text0;
} Instruction;

typedef struct {
    unsigned char* cell_data;
    char* output;
    size_t instruction_idx;
    size_t fake_cycles;
} CellDataSnapshot;

typedef struct {
    char* key;
    int value;
} JumpLabel;

static unsigned char* cell_data;
static CellDataSnapshot* cell_data_snapshots;
static Instruction* instructions;
static size_t instruction_idx = 0;
static size_t instruction_count = 0;
static size_t snapshot_idx = 0;
static size_t last_accessed_cells[MAX_LAST_ACCESSED_CELLS];
static size_t fake_cycles = 0;
static char my_output[MAX_OUTPUT_SIZE];
static char* runtime_file;
static bool is_runtime_file_loaded = false;
static bool is_halted = false;
static JumpLabel* jump_labels = NULL;

void _write(unsigned char val, int idx)
{
    cell_data[idx] = val;
}

void _write_region(unsigned char val, int idx0, int idx1)
{
    for (int i = idx0; i < idx1; i++)
    {
        cell_data[i] = val;
        fake_cycles += 1;
    }
}

void _checkpoint(int idx)
{
    cell_data[idx] = instruction_idx;
}

void _copy_cell_cell(int src, int dest)
{
    cell_data[dest] = cell_data[src];
}

void _copy_cell_indirect(int src, int dest)
{
    cell_data[cell_data[dest]] = cell_data[src];
}

void _copy_indirect_cell(int src, int dest)
{
    cell_data[dest] = cell_data[cell_data[src]];
}

void _copy_indirect_indirect(int src, int dest)
{
    cell_data[cell_data[dest]] = cell_data[cell_data[src]];
}

void _add(int left, int right, int output)
{
    cell_data[output] = cell_data[left] + cell_data[right];
}

void _subtract(int left, int right, int output)
{
    cell_data[output] = cell_data[left] - cell_data[right];
}

void _multiply(int left, int right, int output)
{
    cell_data[output] = cell_data[left] * cell_data[right];
}

void _divide(int left, int right, int output)
{
    cell_data[output] = cell_data[left] / cell_data[right];
}

void _modulo(int left, int right, int output)
{
    cell_data[output] = cell_data[left] % cell_data[right];
}

void _is_equal(int left, int right, int output)
{
    cell_data[output] = cell_data[left] == cell_data[right];
}

void _is_less_than(int left, int right, int output)
{
    cell_data[output] = cell_data[left] < cell_data[right];
}

void _is_greater_than(int left, int right, int output)
{
    cell_data[output] = cell_data[left] > cell_data[right];
}

void _is_either_above_zero(int left, int right, int output)
{
    cell_data[output] = cell_data[left] || cell_data[right];
}

void _is_both_above_zero(int left, int right, int output)
{
    cell_data[output] = cell_data[left] && cell_data[right];
}

void _jump_if_above_zero(const char* jump_label, int idx)
{
    if (cell_data[idx] > 0)
    {
        instruction_idx = shget(jump_labels, jump_label);
    }
}

void _jump_if_zero(const char* jump_label, int idx)
{
    if (cell_data[idx] == 0)
    {
        instruction_idx = shget(jump_labels, jump_label);
    }
}

void _swap(int idx0, int idx1)
{
    int temp = cell_data[idx0];
    cell_data[idx0] = cell_data[idx1];
    cell_data[idx1] = temp;
}

void _print_numbers(int idx0, int idx1)
{
    for (int i = idx0; i <= idx1; i++)
    {
        const char* text = TextFormat("%i,", cell_data[i]);
        unsigned int output_end = TextLength(my_output);
        TextCopy(my_output + output_end, text);
        fake_cycles += 1;
    }
}

void _print_characters(int idx0, int idx1)
{
    unsigned int output_end = TextLength(my_output);
    for (int i = idx0; i <= idx1; i++)
    {
        my_output[output_end++] = cell_data[i];
        fake_cycles += 1;
    }
}

void _print_text(int idx0, int cell_count)
{
    unsigned int output_end = TextLength(my_output);
    for (int i = idx0; i <= cell_count; i++)
    {
        if (cell_data[i] == 0) break;
        my_output[output_end++] = cell_data[i];
        fake_cycles += 1;
    }
}

void _load(const char* file, int cell_count)
{
    if (is_runtime_file_loaded) UnloadFileText(runtime_file);
    runtime_file = LoadFileText(file);
    is_runtime_file_loaded = true;
    size_t len = TextLength(runtime_file)+1;
    for (int i = 1; i < cell_count && i < len; i++)
    {
        cell_data[i] = runtime_file[i-1];
        fake_cycles += 1;
    }
    int max_len = len;
    if (max_len >= cell_count) max_len = cell_count-1;
    cell_data[max_len] = 0;
}

void nop()
{
    Instruction instruction = { .type = INS_NOP, .idx0 = -1, .idx1 = -1, .idx2 = -1 };
    instructions[instruction_count++] = instruction;
}

void write(unsigned char val, int idx)
{
    Instruction instruction = {
        .type = INS_WRITE,
        .val0 = val,
        .idx0 = idx,
        .idx1 = -1,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

void copy_cell_to_cell(int src, int dest)
{
    Instruction instruction = {
        .type = INS_COPY_CELL_CELL,
        .idx0 = src,
        .idx1 = dest,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

void copy_cell_to_indirect(int src, int dest)
{
    Instruction instruction = {
        .type = INS_COPY_CELL_INDIRECT,
        .idx0 = src,
        .idx1 = dest,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

void copy_indirect_to_cell(int src, int dest)
{
    Instruction instruction = {
        .type = INS_COPY_INDIRECT_CELL,
        .idx0 = src,
        .idx1 = dest,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

void copy_indirect_to_indirect(int src, int dest)
{
    Instruction instruction = {
        .type = INS_COPY_INDIRECT_INDIRECT,
        .idx0 = src,
        .idx1 = dest,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

void _left_right_instruction(InstructionType type, int left, int right, int output)
{
    Instruction instruction = {
        .type = type,
        .idx0 = left,
        .idx1 = right,
        .idx2 = output
    };
    instructions[instruction_count++] = instruction;
}

void _two_index_instruction(InstructionType type, int idx0, int idx1)
{
    Instruction instruction = {
        .type = type,
        .idx0 = idx0,
        .idx1 = idx1,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

void add(int left, int right, int output)
{
    _left_right_instruction(INS_ADD, left, right, output);
}

void subtract(int left, int right, int output)
{
    _left_right_instruction(INS_SUB, left, right, output);
}

void multiply(int left, int right, int output)
{
    _left_right_instruction(INS_MUL, left, right, output);
}

void divide(int left, int right, int output)
{
    _left_right_instruction(INS_DIV, left, right, output);
}

void modulo(int left, int right, int output)
{
    _left_right_instruction(INS_MOD, left, right, output);
}

void is_equal(int left, int right, int output)
{
    _left_right_instruction(INS_EQUAL, left, right, output);
}

void is_less_than(int left, int right, int output)
{
    _left_right_instruction(INS_LESS, left, right, output);
}

void is_greater_than(int left, int right, int output)
{
    _left_right_instruction(INS_GREAT, left, right, output);
}

void is_either_above_zero(int left, int right, int output)
{
    _left_right_instruction(INS_OR, left, right, output);
}

void is_both_above_zero(int left, int right, int output)
{
    _left_right_instruction(INS_AND, left, right, output);
}

void jump_if_above_zero(const char* jump_label, int idx)
{
    Instruction instruction = {
        .type = INS_JUMP_IF,
        .text0 = jump_label,
        .idx0 = idx,
        .idx1 = -1,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

void jump_if_zero(const char* jump_label, int idx)
{
    Instruction instruction = {
        .type = INS_JUMP_NOT_IF,
        .text0 = jump_label,
        .idx0 = idx,
        .idx1 = -1,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

void swap(int idx0, int idx1)
{
     _two_index_instruction(INS_SWAP, idx0, idx1);
}

void print_numbers(int idx0, int idx1)
{
    _two_index_instruction(INS_PRINT_NUMBERS, idx0, idx1);
}

void print_characters(int idx0, int idx1)
{
    _two_index_instruction(INS_PRINT_CHARACTERS, idx0, idx1);
}

void print_text(int idx0)
{
    Instruction instruction = {
        .type = INS_PRINT_TEXT,
        .idx0 = idx0,
        .idx1 = -1,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

void write_region(unsigned char val, int idx0, int idx1)
{
    Instruction instruction = {
        .type = INS_WRITE_REGION,
        .val0 = val,
        .idx0 = idx0,
        .idx1 = idx1,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

void load(const char* file)
{
    Instruction instruction = {
        .type = INS_LOAD,
        .text0 = file,
        .idx0 = -1,
        .idx1 = -1,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

void halt()
{
    Instruction instruction = {
        .type = INS_HALT,
        .idx0 = -1,
        .idx1 = -1,
        .idx2 = -1
    };
    instructions[instruction_count++] = instruction;
}

#define and is_both_above_zero
#define or is_either_above_zero
#define label(id, idx) int id = idx; nop();
#define relabel(id, idx) id = idx; nop();
#define dereference(idx0, idx1) copy_indirect_to_cell(idx0, idx1);
#define checkpoint(name) shput(jump_labels, #name, instruction_count); nop();
#define jump_if_above_zero(name, idx) jump_if_above_zero(#name, idx);
#define jump_if_zero(name, idx) jump_if_zero(#name, idx);
#define cell

void execute_instruction(Instruction ins, int cell_count)
{
    const bool is_idx0_mem = (ins.idx0 < cell_count && ins.idx0 != -1);
    const bool is_idx1_mem = (ins.idx1 < cell_count && ins.idx1 != -1);
    const bool is_idx2_mem = (ins.idx2 < cell_count && ins.idx2 != -1);
    if (is_idx0_mem || is_idx1_mem || is_idx2_mem)
    {
        fake_cycles += 1;
    }

    switch(ins.type)
    {
        case INS_WRITE: _write(ins.val0, ins.idx0); break;
        case INS_WRITE_REGION: _write_region(ins.val0, ins.idx0, ins.idx1); break;
        case INS_CHECKPOINT: _checkpoint(ins.idx0); break;
        case INS_COPY_CELL_CELL: _copy_cell_cell(ins.idx0, ins.idx1); break;
        case INS_COPY_CELL_INDIRECT: _copy_cell_indirect(ins.idx0, ins.idx1); break;
        case INS_COPY_INDIRECT_CELL: _copy_indirect_cell(ins.idx0, ins.idx1); break;
        case INS_COPY_INDIRECT_INDIRECT: _copy_indirect_indirect(ins.idx0, ins.idx1); break;
        case INS_ADD: _add(ins.idx0, ins.idx1, ins.idx2); break;
        case INS_SUB: _subtract(ins.idx0, ins.idx1, ins.idx2); break;
        case INS_MUL: _multiply(ins.idx0, ins.idx1, ins.idx2); break;
        case INS_DIV: _divide(ins.idx0, ins.idx1, ins.idx2); break;
        case INS_MOD: _modulo(ins.idx0, ins.idx1, ins.idx2); break;
        case INS_EQUAL: _is_equal(ins.idx0, ins.idx1, ins.idx2); break;
        case INS_LESS: _is_less_than(ins.idx0, ins.idx1, ins.idx2); break;
        case INS_GREAT: _is_greater_than(ins.idx0, ins.idx1, ins.idx2); break;
        case INS_OR: _is_either_above_zero(ins.idx0, ins.idx1, ins.idx2); break;
        case INS_AND: _is_both_above_zero(ins.idx0, ins.idx1, ins.idx2); break;
        case INS_JUMP_IF: _jump_if_above_zero(ins.text0, ins.idx0); break;
        case INS_JUMP_NOT_IF: _jump_if_zero(ins.text0, ins.idx0); break;
        case INS_SWAP: _swap(ins.idx0, ins.idx1); break;
        case INS_PRINT_NUMBERS: _print_numbers(ins.idx0, ins.idx1); break;
        case INS_PRINT_CHARACTERS: _print_characters(ins.idx0, ins.idx1); break;
        case INS_PRINT_TEXT: _print_text(ins.idx0, cell_count); break;
        case INS_LOAD: _load(ins.text0, cell_count); break;
        case INS_HALT: is_halted = true; break;
    }
}

void update_accessed_cells()
{
    last_accessed_cells[0] = instructions[instruction_idx].idx0;
    last_accessed_cells[1] = instructions[instruction_idx].idx1;
    last_accessed_cells[2] = instructions[instruction_idx].idx2;
}

void main()
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Programming Memory");
    SetTargetFPS(30);
    InitAudioDevice();
    Sound click01_sound = LoadSound("click_01.wav");
    SetAudioStreamVolume(click01_sound.stream, 0.01f);
    Sound click02_sound = LoadSound("click_02.wav");
    SetAudioStreamVolume(click02_sound.stream, 0.01f);

    CellDataType cell_data_type = CELL_DATA_NUMBER;
    const int num_registers = 4;
    const int cells_per_row = 12;
    const int cell_rows = 6;
    const int cell_count = cells_per_row * cell_rows;
    const int cell_data_length = cell_count + num_registers;
    cell_data = (unsigned char*)malloc(cell_data_length * sizeof(unsigned char));
    for (int i = 0; i < cell_count; i++) 
    {
        cell_data[i] = GetRandomValue(0, 255);
    }
    cell_data[0] = 0;
    cell_data_snapshots = (CellDataSnapshot*)malloc(MAX_SNAPSHOTS * sizeof(CellDataSnapshot));
    for (int i = 0; i < MAX_SNAPSHOTS; i++) 
    {
        cell_data_snapshots[i].cell_data = (unsigned char*)malloc(cell_data_length * sizeof(unsigned char));
        cell_data_snapshots[i].output = (char*)malloc(MAX_OUTPUT_SIZE * sizeof(char));
        cell_data_snapshots[i].instruction_idx = 0;
        cell_data_snapshots[i].fake_cycles = 0;
    }
    instructions = (Instruction*)malloc(MAX_INSTRUCTIONS * sizeof(Instruction));
    for (int i = 0; i < MAX_OUTPUT_SIZE; i++)
    {
        my_output[i] = 0;
    }

    const int REG_A = cell_count + 0;
    const int REG_B = cell_count + 1;
    const int REG_C = cell_count + 2;
    const int REG_D = cell_count + 3;
    const int all_registers[4] = {REG_A, REG_B, REG_C, REG_D};
    const char all_register_chars[4] = {'A', 'B', 'C', 'D'};
    cell_data[REG_A] = 1;

    const char* code_text = LoadFileText("code.txt");
    #include "code.txt"
    
    update_accessed_cells();
    
    while(!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_SPACE))
        {
            if (cell_data_type == CELL_DATA_NUMBER)
                cell_data_type = CELL_DATA_CHARACTER;
            else
                cell_data_type = CELL_DATA_NUMBER;
        }

        if ((IsKeyPressed(KEY_RIGHT) || IsKeyDown(KEY_DOWN)) && instruction_idx < instruction_count && !is_halted)
        {
            for (int i = 0; i < MAX_LAST_ACCESSED_CELLS; i++)
                last_accessed_cells[i] = -1;

            memcpy(cell_data_snapshots[snapshot_idx].cell_data, cell_data, cell_data_length);
            memcpy(cell_data_snapshots[snapshot_idx].output, my_output, MAX_OUTPUT_SIZE);
            cell_data_snapshots[snapshot_idx].instruction_idx = instruction_idx;
            cell_data_snapshots[snapshot_idx].fake_cycles = fake_cycles;
            snapshot_idx++;

            execute_instruction(instructions[instruction_idx], cell_count);
            instruction_idx++;

            update_accessed_cells();
            PlaySound(click01_sound);
        }
        if ((IsKeyPressed(KEY_LEFT) || IsKeyDown(KEY_UP)) && instruction_idx > 0)
        {
            for (int i = 0; i < MAX_LAST_ACCESSED_CELLS; i++)
                last_accessed_cells[i] = -1;

            snapshot_idx--;
            is_halted = false;
            memcpy(cell_data, cell_data_snapshots[snapshot_idx].cell_data, cell_data_length);
            memcpy(my_output, cell_data_snapshots[snapshot_idx].output, MAX_OUTPUT_SIZE);
            instruction_idx = cell_data_snapshots[snapshot_idx].instruction_idx;
            fake_cycles = cell_data_snapshots[snapshot_idx].fake_cycles;
            update_accessed_cells();

            PlaySound(click02_sound);
        }

        BeginDrawing();
        ClearBackground(BLACK);

        DrawText(FormatText("CLOCK: %08d", snapshot_idx + (fake_cycles*99)), 50, 50, 20, BLUE);

        const int cell_width = 70;
        const int cell_height = 90;
        const int all_cells_x = 500;
        const int all_cells_y = 100;
        const int cell_line_width = 1;
        const int cell_address_line_local_y = 20;
        const int cell_address_font_size = 10;
        const Color cell_color = RAYWHITE;
        const int cell_data_font_size = 30;
        const int all_cells_end_x = all_cells_x + ((cell_width-cell_line_width) * cells_per_row);
        const int all_cells_end_y = all_cells_y + ((cell_height-cell_line_width) * cell_rows);

        const int pixel_width = 6;
        const int pixel_height = 6;
        const int pixels_x = all_cells_end_x - (pixel_width*cells_per_row);
        const int pixels_y = 50;
        DrawRectangleLines(pixels_x-1, pixels_y-1, (cells_per_row*pixel_width)+2, (cell_rows*pixel_height)+2, BLUE);
        for (int i = 0; i < cell_count; i++)
        {
            const int x = i % cells_per_row;
            const int y = i / cells_per_row;
            const Color col = {
                .r = cell_data[i],
                .g = cell_data[i],
                .b = cell_data[i],
                .a = 255
            };
            DrawRectangle(
                pixels_x + (x * pixel_width),
                pixels_y + (y * pixel_height),
                pixel_width,
                pixel_height,
                col
            );
        }

        const char* output_text = FormatText("OUTPUT:\n%s", my_output);
        Rectangle output_text_rec = {
            .x = all_cells_x,
            .y = all_cells_end_y + 10,
            .width = 800,
            .height = 400
        };
        DrawTextRec(GetFontDefault(), output_text, output_text_rec, 10, 2, true, BLUE);

        const int code_x = 50;
        const int code_y = 100;
        const int code_font_size = CODE_FONT_SIZE;
        const int code_line_height = code_font_size/2;
        Rectangle code_line_rect = {
            .x = code_x,
            .y = code_y + (instruction_idx * (code_font_size+code_line_height)),
            .width = 360,
            .height = code_font_size
        };
        DrawRectangleRec(code_line_rect, RED);
        DrawText(code_text, code_x, code_y, code_font_size, RAYWHITE);

        const int register_x_start = 500;
        const int register_x_spacing = 150;
        const int register_y_start = 50;
        const int register_font_size = 40;
        const Color register_font_color = RAYWHITE;
        const int register_text_width = MeasureText("A: 255", register_font_size);
        const int register_highlight_padding = 5;
        Rectangle register_highlight_rec = {
            .x = register_x_start + register_highlight_padding,
            .y = register_y_start + register_highlight_padding,
            .width = register_text_width - (register_highlight_padding*2),
            .height = register_font_size - (register_highlight_padding*2)
        };
        for (int r = 0; r < 4; r++)
        {
            for (int k = 0; k < MAX_LAST_ACCESSED_CELLS; k++)
            {
                if (last_accessed_cells[k] == all_registers[r])
                {
                    Vector2 origin = {.x = -register_x_spacing * r, .y = 0};
                    DrawRectanglePro(register_highlight_rec, origin, 0, (k == 2) ? GREEN : RED);
                }
            }
            DrawText(FormatText("%c: %03d", all_register_chars[r], cell_data[all_registers[r]]), 
                register_x_start + (register_x_spacing*r), 
                register_y_start, 
                register_font_size, 
                register_font_color);
        }

        // Draw Cells.
        for (int i = 0; i < cell_count; i++)
        {
            Rectangle box_rect = {
                .x = all_cells_x + ((i % cells_per_row) * (cell_width-cell_line_width)), 
                .y = all_cells_y + ((i / cells_per_row) * (cell_height-cell_line_width)), 
                .width = cell_width, 
                .height = cell_height};
            const int half_width = box_rect.width/2;
            const int half_height = box_rect.height/2;
            DrawRectangleLinesEx(box_rect, cell_line_width, cell_color);

            const int fill_padding = 10;
            Rectangle fill_box_rec = {
                .x = box_rect.x + fill_padding,
                .y = box_rect.y + cell_address_line_local_y + fill_padding,
                .width = box_rect.width - fill_padding*2,
                .height = box_rect.height - cell_address_line_local_y - fill_padding*2
            };

            for (int k = 0; k < MAX_LAST_ACCESSED_CELLS; k++)
            {
                if (i == last_accessed_cells[k] && i != 0)
                {
                    DrawRectangleRec(fill_box_rec, (k == 2) ? GREEN : RED);
                }

                if (instruction_idx >= instruction_count) continue;

                const InstructionType itype = instructions[instruction_idx].type;
                const bool is_idx0_indirect = (itype == INS_COPY_INDIRECT_CELL || itype == INS_COPY_INDIRECT_INDIRECT);
                const bool is_idx1_indirect = (itype == INS_COPY_CELL_INDIRECT || itype == INS_COPY_INDIRECT_INDIRECT);
                const int idx0_ref = instructions[instruction_idx].idx0;
                const bool is_idx0_ref_this_cell = (i == cell_data[idx0_ref]);
                const bool is_idx0_a_register = (idx0_ref >= cell_count);
                const int idx1_ref = instructions[instruction_idx].idx1;
                const bool is_idx1_ref_this_cell = (i == cell_data[idx1_ref]);
                const bool is_idx1_a_register = (idx1_ref >= cell_count);
                if (    (is_idx0_indirect && is_idx0_ref_this_cell && is_idx0_a_register)
                     || (is_idx1_indirect && is_idx1_ref_this_cell && is_idx1_a_register)
                )
                {
                    const int register_idx = (is_idx0_ref_this_cell) ? (idx0_ref - cell_count) : (idx1_ref - cell_count);
                    DrawRectangleRec(fill_box_rec, BLUE);
                    Vector2 register_origin = {.x = register_x_spacing * register_idx, .y = register_highlight_rec.height };
                    DrawLine(
                        register_highlight_rec.x + register_origin.x,
                        register_highlight_rec.y + register_origin.y,
                        fill_box_rec.x, fill_box_rec.y, BLUE);
                }
                else if ( (is_idx0_indirect && is_idx0_ref_this_cell)
                        ||(is_idx1_indirect && is_idx1_ref_this_cell)        
                )
                {
                    const int ref_idx = (is_idx0_ref_this_cell) ? idx0_ref : idx1_ref;
                    const int x = all_cells_x + ((ref_idx % cells_per_row) * (cell_width-cell_line_width));
                    const int y = all_cells_y + ((ref_idx / cells_per_row) * (cell_height-cell_line_width));
                    DrawRectangleRec(fill_box_rec, BLUE);
                    DrawLine(x + (cell_width/2), y + (cell_height/2), fill_box_rec.x, fill_box_rec.y, BLUE);
                }
            }

            Vector2 line_vec_left = {
                .x = box_rect.x,
                .y = box_rect.y + cell_address_line_local_y
            };
            Vector2 line_vec_right = {
                .x = box_rect.x + box_rect.width,
                .y = box_rect.y + cell_address_line_local_y
            };
            DrawLineEx(line_vec_left, line_vec_right, 1, cell_color);
            const char* address_text = TextFormat("%02d", i);
            const int address_text_width = MeasureText(address_text, cell_address_font_size);
            DrawText(address_text,
                box_rect.x + half_width - (address_text_width/2),
                box_rect.y + (cell_address_line_local_y/2) - (cell_address_font_size/2),
                cell_address_font_size,
                cell_color
            );

            const int data_x = box_rect.x + half_width;
            const int data_y = box_rect.y + cell_address_line_local_y + ((box_rect.height-cell_address_line_local_y)/2);
            if (i == 0)
            {
                DrawText("!", data_x, data_y-15, 30, RED);
                continue;
            }
            switch(cell_data_type) {
                case CELL_DATA_NUMBER: {
                    const char* data_text = TextFormat("%d", cell_data[i]);
                    int data_text_width = MeasureText(data_text, cell_data_font_size);
                    DrawText(data_text, 
                        data_x - (data_text_width/2),
                        data_y - (cell_data_font_size/2),
                        cell_data_font_size,
                        cell_color);
                    break;
                }
                case CELL_DATA_CHARACTER: {
                    const char* data_text = TextFormat("%c", cell_data[i]);
                    int data_text_width = MeasureText(data_text, cell_data_font_size);
                    DrawText(data_text, 
                        data_x - (data_text_width/2),
                        data_y - (cell_data_font_size/2),
                        cell_data_font_size,
                        cell_color);
                    break;
                }
            }
        }

        // const float total_frame_duration = GetFrameTime();
        // DrawText(
        //     FormatText("Frame time: %.03f%%", 100.0f * (1.0f / (total_frame_duration * 30.f))),
        //     10, 10,
        //     20,
        //     RED);

        EndDrawing();
    }

    CloseAudioDevice();
    CloseWindow();
}