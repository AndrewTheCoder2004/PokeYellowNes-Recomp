#ifndef THUNDER_CORE_H
#define THUNDER_CORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NES_SCREEN_WIDTH  256
#define NES_SCREEN_HEIGHT 240
#define NES_FPS           60

/* Standard NES Controller bitmask */
enum {
    NES_BTN_A      = (1 << 0),
    NES_BTN_B      = (1 << 1),
    NES_BTN_SELECT = (1 << 2),
    NES_BTN_START  = (1 << 3),
    NES_BTN_UP     = (1 << 4),
    NES_BTN_DOWN   = (1 << 5),
    NES_BTN_LEFT   = (1 << 6),
    NES_BTN_RIGHT  = (1 << 7)
};

/* Virtual unified input representation */
typedef struct {
    uint8_t pad[2]; /* NES standard controller bitmasks for player 1 & 2 */
} ThunderInput;

/* Mapper 163 (Nanjing / Waixing / Lei Dian Huang) state */
typedef struct {
    uint8_t reg[8];
    uint8_t prg_bank_lo;
    uint8_t prg_bank_hi;
    uint8_t security_val;
    uint8_t strobe;
} Mapper163;

/* Core state struct designed for zero-allocation and instant serialization */
typedef struct {
    /* 6502 CPU State */
    uint16_t pc;
    uint8_t  a, x, y, sp, p;
    uint64_t cycles;
    bool     nmi_pending;
    bool     irq_pending;

    /* Memory buffers */
    uint8_t  ram[0x800];        /* 2KB internal CPU RAM */
    uint8_t  sram[0x2000];      /* 8KB PRG RAM / Battery Save */
    uint8_t  vram[0x800];       /* 2KB internal nametable RAM */
    uint8_t  chr_ram[0x2000];   /* 8KB CHR-RAM (Mapper 163 uses CHR-RAM) */
    uint8_t  palette[32];       /* 32 bytes palette memory */
    uint8_t  oam[256];          /* 256 bytes sprite OAM */

    /* PPU State */
    uint16_t ppu_v;             /* Current VRAM address (15 bits) */
    uint16_t ppu_t;             /* Temporary VRAM address (15 bits) */
    uint8_t  ppu_x;             /* Fine X scroll (3 bits) */
    uint8_t  ppu_w;             /* First/second write toggle */
    uint8_t  ppu_ctrl;
    uint8_t  ppu_mask;
    uint8_t  ppu_status;
    uint8_t  ppu_oam_addr;
    uint8_t  ppu_data_buf;
    int      scanline;
    int      cycle;

    /* Controllers */
    uint8_t  pad_state[2];
    uint8_t  pad_shift[2];
    uint8_t  pad_strobe;

    /* Mapper 163 */
    Mapper163 mapper;

    /* PRG ROM pointer and size */
    const uint8_t *prg_rom;
    uint32_t       prg_size;

    /* Video Framebuffer: 256x240 ARGB8888 / RGB565 friendly 32-bit output */
    uint32_t framebuffer[NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT];
} ThunderCore;

/* Core API */
void thunder_init(ThunderCore *core, const uint8_t *rom_data, uint32_t rom_size);
void thunder_reset(ThunderCore *core);
void thunder_set_input(ThunderCore *core, uint8_t p1_buttons, uint8_t p2_buttons);
void thunder_run_frame(ThunderCore *core);

/* Audio callback hook for APU rendering */
typedef void (*ThunderAudioCallback)(void *userdata, const int16_t *samples, int count);
void thunder_set_audio_callback(ThunderCore *core, ThunderAudioCallback cb, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* THUNDER_CORE_H */
