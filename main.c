#include "thunder_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef HAVE_SDL2
#include <SDL.h>

static uint8_t map_sdl_input(const uint8_t *keystates, SDL_GameController *pad) {
    uint8_t btns = 0;
    if (keystates[SDL_SCANCODE_W] || keystates[SDL_SCANCODE_UP])     btns |= NES_BTN_UP;
    if (keystates[SDL_SCANCODE_S] || keystates[SDL_SCANCODE_DOWN])   btns |= NES_BTN_DOWN;
    if (keystates[SDL_SCANCODE_A] || keystates[SDL_SCANCODE_LEFT])   btns |= NES_BTN_LEFT;
    if (keystates[SDL_SCANCODE_D] || keystates[SDL_SCANCODE_RIGHT])  btns |= NES_BTN_RIGHT;
    if (keystates[SDL_SCANCODE_K] || keystates[SDL_SCANCODE_X])      btns |= NES_BTN_A;
    if (keystates[SDL_SCANCODE_J] || keystates[SDL_SCANCODE_Z])      btns |= NES_BTN_B;
    if (keystates[SDL_SCANCODE_RETURN])                              btns |= NES_BTN_START;
    if (keystates[SDL_SCANCODE_RSHIFT] || keystates[SDL_SCANCODE_BACKSPACE]) btns |= NES_BTN_SELECT;

    if (pad) {
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))    btns |= NES_BTN_UP;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  btns |= NES_BTN_DOWN;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  btns |= NES_BTN_LEFT;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) btns |= NES_BTN_RIGHT;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A))          btns |= NES_BTN_A;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B))          btns |= NES_BTN_B;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START))      btns |= NES_BTN_START;
        if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK))       btns |= NES_BTN_SELECT;
    }
    return btns;
}
#endif

int main(int argc, char *argv[]) {
    const char *rom_path = (argc > 1) ? argv[1] : "Lei Dian Huang - Bi Ka Qiu Chuan Shuo (China)(Unlicensed).nes";
    FILE *f = fopen(rom_path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open ROM: %s\n", rom_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *rom_buf = (uint8_t *)malloc(sz);
    if (!rom_buf || fread(rom_buf, 1, sz, f) != (size_t)sz) {
        fprintf(stderr, "Failed to read ROM into memory\n");
        fclose(f);
        return 1;
    }
    fclose(f);

    ThunderCore core;
    thunder_init(&core, rom_buf, (uint32_t)sz);
    printf("Initialized ThunderCore with ROM size %ld bytes. Reset PC: 0x%04X\n", sz, core.pc);

#ifdef HAVE_SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("The Thunder Emperor - Recomp",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        NES_SCREEN_WIDTH * 3, NES_SCREEN_HEIGHT * 3, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NES_SCREEN_WIDTH, NES_SCREEN_HEIGHT);

    SDL_GameController *pad = NULL;
    if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
        pad = SDL_GameControllerOpen(0);
    }

    bool running = true;
    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
        }

        const uint8_t *keys = SDL_GetKeyboardState(NULL);
        uint8_t p1 = map_sdl_input(keys, pad);
        thunder_set_input(&core, p1, 0);

        thunder_run_frame(&core);

        SDL_UpdateTexture(tex, NULL, core.framebuffer, NES_SCREEN_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    if (pad) SDL_GameControllerClose(pad);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
#else
    /* Non-GUI benchmark / verification run */
    for (int i = 0; i < 60; i++) {
        thunder_run_frame(&core);
    }
    printf("Headless verification: Successfully executed 60 frames.\n");
#endif

    free(rom_buf);
    return 0;
}
