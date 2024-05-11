#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "chip8.h"

#define SCALE_FACTOR 10

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file.c8>\n", argv[0]);
        return 1;
    }

    Chip8 chip8;
    chip8_initialize(&chip8);
    chip8_loadGame(&chip8, argv[1]);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DISPLAY_WIDTH * SCALE_FACTOR, DISPLAY_HEIGHT * SCALE_FACTOR, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    uint32_t pixels[DISPLAY_WIDTH * DISPLAY_HEIGHT];

    bool quit = false;
    SDL_Event e;
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                quit = true;
            } else {
                chip8_setKeys(&chip8, e);
            }
        }

        chip8_emulateCycle(&chip8);

        if (chip8.drawFlag) {
            for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
                pixels[i] = (chip8.display[i] == 0) ? 0x00000000 : 0xFFFFFFFF;
            }
            SDL_UpdateTexture(texture, NULL, pixels, DISPLAY_WIDTH * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            chip8.drawFlag = false;
        }

        SDL_Delay(1);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
