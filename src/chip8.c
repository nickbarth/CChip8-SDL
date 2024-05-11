#include "chip8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

const SDL_Keycode keymap[16] = {
    SDLK_1, SDLK_2, SDLK_3, SDLK_4,
    SDLK_q, SDLK_w, SDLK_e, SDLK_r,
    SDLK_a, SDLK_s, SDLK_d, SDLK_f,
    SDLK_z, SDLK_x, SDLK_c, SDLK_v
};

// character fontset
unsigned char chip8_fontset[80] =
{
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void chip8_initialize(Chip8* chip8) {
    // 4k memory
    //   0x000-0x1FF - chip 8 interpreter (contains font set in emu)
    //   0x050-0x0A0 - used for the built in 4x5 pixel font set (0-F)
    //   0x200-0xFFF - program ROM and work RAM

    chip8->pc = 0x200;  // program counter starts at 0x200
    chip8->sp = 0;      // stack pointer
    chip8->I = 0;       // index register
    memset(chip8->memory, 0, MEMORY_SIZE);
    memset(chip8->V, 0, NUM_REGISTERS);
    memset(chip8->stack, 0, STACK_SIZE * sizeof(uint16_t));
    memset(chip8->display, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT);
    memset(chip8->keypad, 0, NUM_KEYS);

    chip8->delay_timer = 0;
    chip8->sound_timer = 0;
    chip8->cycle_count = 0;

    // load fontset
    for (int i = 0; i < 80; ++i) {
        chip8->memory[i] = chip8_fontset[i];
    }
}

void chip8_loadGame(Chip8* chip8, const char* filename) {
    FILE* game = fopen(filename, "rb");
    if (!game) {
        fprintf(stderr, "Failed to open file\n");
        return;
    }

    size_t bytesRead = fread(chip8->memory + 0x200, sizeof(uint8_t), MEMORY_SIZE - 0x200, game);
    if (bytesRead <= 0) {
        fprintf(stderr, "Failed to load file\n");
    }

    fclose(game);
}

void chip8_setKeys(Chip8* chip8, SDL_Event event) {
    for (int i = 0; i < 16; ++i) {
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == keymap[i]) {
            chip8->keypad[i] = true;
        }
        if (event.type == SDL_KEYUP && event.key.keysym.sym == keymap[i]) {
            chip8->keypad[i] = false;
        }
    }
}

void chip8_emulateCycle(Chip8* chip8) {
    uint16_t opcode = chip8->memory[chip8->pc] << 8 | chip8->memory[chip8->pc + 1];

    // debugging
    /*
    printf("\n-- cycle: %d -- \n", chip8->cycle_count);
    printf("opcode: 0x%04X\n", opcode);
    printf("pc: 0x%04X\n", chip8->pc);
    printf("i: 0x%04X\n", chip8->I);
    printf("sp: 0x%02X\n", chip8->sp);
    */

    chip8->pc += 2;
    chip8->cycle_count++;

    switch (opcode & 0xF000) {
        case 0x0000:
            switch (opcode & 0x00FF) {
                case 0x00E0:  // CLS - Clear the display
                    memset(chip8->display, 0, sizeof(chip8->display));
                    chip8->drawFlag = true;
                    break;
                case 0x00EE:  // RET - Return from a subroutine
                    chip8->pc = chip8->stack[--chip8->sp];
                    break;
                default:
                    fprintf(stderr, "Unknown opcode: 0x%X\n", opcode);
                    exit(EXIT_FAILURE);
            }
            break;
        case 0x1000:  // JP addr - Jump to location nnn
            chip8->pc = opcode & 0x0FFF;
            break;
        case 0x2000:  // CALL addr - Call subroutine at nnn
            chip8->stack[chip8->sp++] = chip8->pc;
            chip8->pc = opcode & 0x0FFF;
            break;
        case 0x3000:  // SE Vx, byte - Skip next instruction if Vx = kk
            if (chip8->V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF))
                chip8->pc += 2;
            break;
        case 0x4000:  // SNE Vx, byte - Skip next instruction if Vx != kk
            if (chip8->V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
                chip8->pc += 2;
            break;
        case 0x5000:  // SE Vx, Vy - Skip next instruction if Vx = Vy
            if (chip8->V[(opcode & 0x0F00) >> 8] == chip8->V[(opcode & 0x00F0) >> 4])
                chip8->pc += 2;
            break;
        case 0x6000:  // LD Vx, byte - Set Vx = kk
            chip8->V[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
            break;
        case 0x7000:  // ADD Vx, byte - Set Vx = Vx + kk
            chip8->V[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
            break;
        case 0x8000: {
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;
            switch (opcode & 0x000F) {
                case 0x0000:  // LD Vx, Vy - Set Vx = Vy
                    chip8->V[x] = chip8->V[y];
                    break;
                case 0x0001:  // OR Vx, Vy - Set Vx = Vx OR Vy
                    chip8->V[x] |= chip8->V[y];
                    break;
                case 0x0002:  // AND Vx, Vy - Set Vx = Vx AND Vy
                    chip8->V[x] &= chip8->V[y];
                    break;
                case 0x0003:  // XOR Vx, Vy - Set Vx = Vx XOR Vy
                    chip8->V[x] ^= chip8->V[y];
                    break;
                case 0x0004:  // ADD Vx, Vy - Set Vx = Vx + Vy, set VF = carry
                    chip8->V[0xF] = (chip8->V[x] + chip8->V[y] > 255) ? 1 : 0;
                    chip8->V[x] += chip8->V[y];
                    break;
                case 0x0005:  // SUB Vx, Vy - Set Vx = Vx - Vy, set VF = NOT borrow
                    chip8->V[0xF] = (chip8->V[x] > chip8->V[y]) ? 1 : 0;
                    chip8->V[x] -= chip8->V[y];
                    break;
                case 0x0006:  // SHR Vx {, Vy} - Set Vx = Vx SHR 1
                    chip8->V[0xF] = chip8->V[x] & 0x1;
                    chip8->V[x] >>= 1;
                    break;
                case 0x0007:  // SUBN Vx, Vy - Set Vx = Vy - Vx, set VF = NOT borrow
                    chip8->V[0xF] = (chip8->V[y] > chip8->V[x]) ? 1 : 0;
                    chip8->V[x] = chip8->V[y] - chip8->V[x];
                    break;
                case 0x000E:  // SHL Vx {, Vy} - Set Vx = Vx SHL 1
                    chip8->V[0xF] = chip8->V[x] >> 7;
                    chip8->V[x] <<= 1;
                    break;
                default:
                    fprintf(stderr, "Unknown opcode 0: 0x%X\n", opcode);
                    exit(EXIT_FAILURE);
                }
            }
            break;
        case 0x9000:  // SNE Vx, Vy - Skip next instruction if Vx != Vy
            if (chip8->V[(opcode & 0x0F00) >> 8] != chip8->V[(opcode & 0x00F0) >> 4])
                chip8->pc += 2;
            break;
        case 0xA000:  // LD I, addr - Set I = nnn
            chip8->I = opcode & 0x0FFF;
            break;
        case 0xB000:  // JP V0, addr - Jump to location nnn + V0
            chip8->pc = (opcode & 0x0FFF) + chip8->V[0];
            break;
        case 0xC000:  // RND Vx, byte - Set Vx = random byte AND kk
            chip8->V[(opcode & 0x0F00) >> 8] = (rand() % 256) & (opcode & 0x00FF);
            break;
        case 0xD000: { // DRW Vx, Vy, nibble - Display n-byte sprite starting at memory location I at (Vx, Vy)
                uint8_t x = chip8->V[(opcode & 0x0F00) >> 8];
                uint8_t y = chip8->V[(opcode & 0x00F0) >> 4];
                uint8_t height = opcode & 0x000F;
                chip8->V[0xF] = 0;
                for (int j = 0; j < height; j++) {
                    uint8_t sprite = chip8->memory[chip8->I + j];
                    for (int i = 0; i < 8; i++) {
                        if ((sprite & (0x80 >> i)) != 0) {
                            int index = (x + i + ((y + j) * DISPLAY_WIDTH)) % (DISPLAY_WIDTH * DISPLAY_HEIGHT);
                            if (chip8->display[index] == 1)
                                chip8->V[0xF] = 1;
                            chip8->display[index] ^= 1;
                        }
                    }
                }
                chip8->drawFlag = true;
            }
            break;
        case 0xE000:
            switch (opcode & 0x00FF) {
                case 0x009E:  // SKP Vx - Skip next instruction if key with the value of Vx is pressed
                    if (chip8->keypad[chip8->V[(opcode & 0x0F00) >> 8]])
                        chip8->pc += 2;
                    break;
                case 0x00A1:  // SKNP Vx - Skip next instruction if key with the value of Vx is not pressed
                    if (!chip8->keypad[chip8->V[(opcode & 0x0F00) >> 8]])
                        chip8->pc += 2;
                    break;
                default:
                    fprintf(stderr, "Unknown opcode 1: 0x%X\n", opcode);
                    exit(EXIT_FAILURE);
            }
            break;
        case 0xF000:
            switch (opcode & 0x00FF) {
                case 0x0007:  // LD Vx, DT - Set Vx = delay timer value
                    chip8->V[(opcode & 0x0F00) >> 8] = chip8->delay_timer;
                    break;
                case 0x000A: { // LD Vx, K - Wait for a key press, store the value of the key in Vx
                        bool keyPressed = false;
                        for (int i = 0; i < 16; ++i) {
                            if (chip8->keypad[i]) {
                                chip8->V[(opcode & 0x0F00) >> 8] = i;
                                keyPressed = true;
                            }
                        }
                        if (!keyPressed) {
                            return;
                        }
                    }
                    break;
                case 0x0015:  // LD DT, Vx - Set delay timer = Vx
                    chip8->delay_timer = chip8->V[(opcode & 0x0F00) >> 8];
                    break;
                case 0x0018:  // LD ST, Vx - Set sound timer = Vx
                    chip8->sound_timer = chip8->V[(opcode & 0x0F00) >> 8];
                    break;
                case 0x001E:  // ADD I, Vx - Add Vx to I
                    chip8->I += chip8->V[(opcode & 0x0F00) >> 8];
                    break;
                case 0x0029:  // LD F, Vx - Set I = location of sprite for digit Vx
                    chip8->I = chip8->V[(opcode & 0x0F00) >> 8] * 5;
                    break;
                case 0x0033: { // LD B, Vx - Store BCD representation of Vx in memory locations I, I+1, and I+2
                        uint8_t value = chip8->V[(opcode & 0x0F00) >> 8];
                        chip8->memory[chip8->I] = value / 100;
                        chip8->memory[chip8->I + 1] = (value / 10) % 10;
                        chip8->memory[chip8->I + 2] = value % 10;
                    }
                    break;
                case 0x0055:  // LD [I], Vx - Store registers V0 through Vx in memory starting at location I
                    for (int i = 0; i <= ((opcode & 0x0F00) >> 8); ++i) {
                        chip8->memory[chip8->I + i] = chip8->V[i];
                    }
                    break;
                case 0x0065:  // LD Vx, [I] - Read registers V0 through Vx from memory starting at location I
                    for (int i = 0; i <= ((opcode & 0x0F00) >> 8); ++i) {
                        chip8->V[i] = chip8->memory[chip8->I + i];
                    }
                    break;
                default:
                    fprintf(stderr, "Unknown opcode 2: 0x%X\n", opcode);
                    exit(EXIT_FAILURE);
            }
            break;
        default:
            fprintf(stderr, "Unknown opcode 3: 0x%X\n", opcode);
            exit(EXIT_FAILURE);
    }

    if (chip8->delay_timer > 0) --chip8->delay_timer;
    if (chip8->sound_timer > 0) --chip8->sound_timer;

    // cycle debugging
    // if (chip8->cycle_count == 21) { exit(0); }
}
