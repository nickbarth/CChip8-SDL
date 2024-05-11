#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>
#include <stdbool.h> 
#include <SDL.h>

#define MEMORY_SIZE 4096
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define NUM_REGISTERS 16
#define STACK_SIZE 16
#define NUM_KEYS 16

typedef struct chip8 {
    uint8_t memory[MEMORY_SIZE];
    uint8_t V[NUM_REGISTERS];
    uint16_t I;
    uint16_t pc;
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint8_t display[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    uint16_t stack[STACK_SIZE];
    uint8_t sp;
    bool keypad[NUM_KEYS];
    bool drawFlag;
    unsigned int cycle_count;
} Chip8;

void chip8_initialize(Chip8* chip8);
void chip8_loadGame(Chip8* chip8, const char* filename);
void chip8_setKeys(Chip8* chip8, SDL_Event event);
void chip8_emulateCycle(Chip8* chip8);

#endif
