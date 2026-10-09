#include "thunder_core.h"
#include <string.h>

/* Standard NES 64-color palette in 0xAARRGGBB format */
static const uint32_t s_nes_palette[64] = {
    0xFF666666, 0xFF002A88, 0xFF1412A7, 0xFF3B00A4, 0xFF5C007E, 0xFF6E0040, 0xFF6C0600, 0xFF561D00,
    0xFF333500, 0xFF0B4800, 0xFF005200, 0xFF004F08, 0xFF00404D, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFADADAD, 0xFF155FD9, 0xFF4240FF, 0xFF7527FE, 0xFFA01ACC, 0xFFB71E7B, 0xFFB53120, 0xFF994E00,
    0xFF6B6D00, 0xFF388700, 0xFF0C9300, 0xFF008F32, 0xFF007C8D, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFF64B0FF, 0xFF9290FF, 0xFFC676FF, 0xFFF36AFF, 0xFFFE6ECC, 0xFFFE8170, 0xFFEA9E22,
    0xFFBCBE00, 0xFF88D800, 0xFF5CE430, 0xFF45E082, 0xFF48CDDE, 0xFF4F4F4F, 0xFF000000, 0xFF000000,
    0xFFFFFEFF, 0xFFC0DFFF, 0xFFD3D2FF, 0xFFE8C8FF, 0xFFFBC2FF, 0xFFFEC4EA, 0xFFFECCC5, 0xFFF7D8A5,
    0xFFE4E594, 0xFFCFEF96, 0xFFBDF4AB, 0xFFB3F3CC, 0xFFB5EBF2, 0xFFB8B8B8, 0xFF000000, 0xFF000000
};

static inline uint8_t cpu_read8(ThunderCore *c, uint16_t addr);
static inline void cpu_write8(ThunderCore *c, uint16_t addr, uint8_t val);

/* Mapper 163 Memory Handling */
static uint8_t mapper163_read(ThunderCore *c, uint16_t addr) {
    if (addr >= 0x5000 && addr <= 0x5FFF) {
        /* Nanjing protection / security read register */
        switch (addr) {
            case 0x5101:
            case 0x5501:
                return c->mapper.security_val;
            case 0x5100:
                return 0x00;
            default:
                return 0x04;
        }
    }
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        return c->sram[addr - 0x6000];
    }
    if (addr >= 0x8000) {
        /* Mapper 163 PRG banking: 32KB bank switchable */
        uint32_t bank = (c->mapper.reg[0] & 0x0F) | ((c->mapper.reg[2] & 0x0F) << 4);
        uint32_t offset = (bank * 0x8000) + (addr - 0x8000);
        if (c->prg_rom && offset < c->prg_size) {
            return c->prg_rom[offset];
        }
    }
    return 0xFF;
}

static void mapper163_write(ThunderCore *c, uint16_t addr, uint8_t val) {
    if (addr >= 0x5000 && addr <= 0x53FF) {
        c->mapper.reg[addr & 0x03] = val;
        if ((addr & 0x03) == 0) {
            /* Bank switch register */
            c->mapper.prg_bank_lo = val;
        } else if ((addr & 0x03) == 2) {
            c->mapper.prg_bank_hi = val;
        }
        /* Protection algorithm for Nanjing/Waixing */
        c->mapper.security_val = (val ^ 0xFF);
    } else if (addr >= 0x6000 && addr <= 0x7FFF) {
        c->sram[addr - 0x6000] = val;
    }
}

static inline uint8_t cpu_read8(ThunderCore *c, uint16_t addr) {
    if (addr < 0x2000) {
        return c->ram[addr & 0x7FF];
    } else if (addr < 0x4000) {
        /* PPU Registers */
        switch (addr & 0x2007) {
            case 0x2002: {
                uint8_t st = c->ppu_status;
                c->ppu_status &= ~0x80; /* Clear VBL flag */
                c->ppu_w = 0;
                return st;
            }
            case 0x2004:
                return c->oam[c->ppu_oam_addr];
            case 0x2007: {
                uint8_t res = c->ppu_data_buf;
                uint16_t v = c->ppu_v & 0x3FFF;
                if (v < 0x2000) {
                    c->ppu_data_buf = c->chr_ram[v];
                } else if (v < 0x3F00) {
                    c->ppu_data_buf = c->vram[v & 0x7FF];
                } else {
                    c->ppu_data_buf = c->palette[v & 0x1F];
                    res = c->ppu_data_buf;
                }
                c->ppu_v += (c->ppu_ctrl & 0x04) ? 32 : 1;
                return res;
            }
            default:
                return 0;
        }
    } else if (addr == 0x4016) {
        /* Controller 1 */
        uint8_t val = (c->pad_shift[0] & 1);
        c->pad_shift[0] >>= 1;
        return val | 0x40;
    } else if (addr == 0x4017) {
        /* Controller 2 */
        uint8_t val = (c->pad_shift[1] & 1);
        c->pad_shift[1] >>= 1;
        return val | 0x40;
    } else if (addr >= 0x5000) {
        return mapper163_read(c, addr);
    }
    return 0;
}

static inline void cpu_write8(ThunderCore *c, uint16_t addr, uint8_t val) {
    if (addr < 0x2000) {
        c->ram[addr & 0x7FF] = val;
    } else if (addr < 0x4000) {
        switch (addr & 0x2007) {
            case 0x2000:
                c->ppu_ctrl = val;
                c->ppu_t = (c->ppu_t & 0xF3FF) | ((val & 0x03) << 10);
                break;
            case 0x2001:
                c->ppu_mask = val;
                break;
            case 0x2003:
                c->ppu_oam_addr = val;
                break;
            case 0x2004:
                c->oam[c->ppu_oam_addr++] = val;
                break;
            case 0x2005: /* Scroll */
                if (c->ppu_w == 0) {
                    c->ppu_t = (c->ppu_t & 0x7FE0) | (val >> 3);
                    c->ppu_x = val & 0x07;
                    c->ppu_w = 1;
                } else {
                    c->ppu_t = (c->ppu_t & 0x0C1F) | ((val & 0x07) << 12) | ((val & 0xF8) << 2);
                    c->ppu_w = 0;
                }
                break;
            case 0x2006: /* PPU Addr */
                if (c->ppu_w == 0) {
                    c->ppu_t = (c->ppu_t & 0x00FF) | ((val & 0x3F) << 8);
                    c->ppu_w = 1;
                } else {
                    c->ppu_t = (c->ppu_t & 0xFF00) | val;
                    c->ppu_v = c->ppu_t;
                    c->ppu_w = 0;
                }
                break;
            case 0x2007: {
                uint16_t v = c->ppu_v & 0x3FFF;
                if (v < 0x2000) {
                    c->chr_ram[v] = val;
                } else if (v < 0x3F00) {
                    c->vram[v & 0x7FF] = val;
                } else {
                    c->palette[v & 0x1F] = val;
                }
                c->ppu_v += (c->ppu_ctrl & 0x04) ? 32 : 1;
                break;
            }
        }
    } else if (addr == 0x4014) {
        /* OAM DMA */
        uint16_t src = ((uint16_t)val) << 8;
        for (int i = 0; i < 256; i++) {
            c->oam[(c->ppu_oam_addr + i) & 0xFF] = cpu_read8(c, src + i);
        }
        c->cycles += 513;
    } else if (addr == 0x4016) {
        if (val & 1) {
            c->pad_strobe = 1;
        } else if (c->pad_strobe) {
            c->pad_strobe = 0;
            c->pad_shift[0] = c->pad_state[0];
            c->pad_shift[1] = c->pad_state[1];
        }
    } else if (addr >= 0x5000) {
        mapper163_write(c, addr, val);
    }
}

void thunder_init(ThunderCore *core, const uint8_t *rom_data, uint32_t rom_size) {
    memset(core, 0, sizeof(ThunderCore));
    /* Locate PRG ROM after iNES 16-byte header */
    if (rom_data && rom_size >= 16) {
        core->prg_rom = rom_data + 16;
        core->prg_size = rom_size - 16;
    }
    thunder_reset(core);
}

void thunder_reset(ThunderCore *core) {
    core->sp = 0xFD;
    core->p  = 0x24;
    core->pc = cpu_read8(core, 0xFFFC) | (cpu_read8(core, 0xFFFD) << 8);
    core->cycles = 0;
    core->ppu_status = 0;
}

void thunder_set_input(ThunderCore *core, uint8_t p1_buttons, uint8_t p2_buttons) {
    core->pad_state[0] = p1_buttons;
    core->pad_state[1] = p2_buttons;
}

/* Fast scanline rasterizer */
static void render_scanline(ThunderCore *c, int line) {
    if (line < 0 || line >= NES_SCREEN_HEIGHT) return;
    uint32_t *dest = &c->framebuffer[line * NES_SCREEN_WIDTH];
    uint32_t bg_color = s_nes_palette[c->palette[0] & 0x3F];
    for (int x = 0; x < NES_SCREEN_WIDTH; x++) {
        dest[x] = bg_color;
    }
}

void thunder_run_frame(ThunderCore *core) {
    /* Execute 262 scanlines per NTSC frame */
    for (int line = 0; line < 262; line++) {
        core->scanline = line;
        if (line < NES_SCREEN_HEIGHT) {
            if (core->ppu_mask & 0x08) { /* If background enabled */
                render_scanline(core, line);
            }
        } else if (line == 241) {
            /* VBLANK start */
            core->ppu_status |= 0x80;
            if (core->ppu_ctrl & 0x80) {
                core->nmi_pending = true;
            }
        } else if (line == 261) {
            /* Pre-render scanline: clear VBLANK */
            core->ppu_status &= ~0x80;
            core->nmi_pending = false;
        }
    }
}
