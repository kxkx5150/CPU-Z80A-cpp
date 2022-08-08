#include "vdp.h"
#include <cstdint>
#include <sys/types.h>
#include "PC.h"

VDP::VDP(PC *_core)
{
    core = _core;
    vdp_reset();
}
void VDP::vdp_writeaddr(int64_t val)
{
    if (vdp_addr_state == 0) {
        vdp_addr_state = 1;
        vdp_addr_latch = val;
    } else {
        vdp_addr_state = 0;

        u_int64_t valu = val;
        switch (valu >> 6) {
            case 0:
            case 1:
                vdp_mode_select = 0;
                vdp_addr        = vdp_addr_latch | ((val & 0x3f) << 8);
                break;
            case 2: {
                int64_t regnum   = val & 0xf;
                vdp_regs[regnum] = vdp_addr_latch;
                switch (regnum) {
                    case 7:
                        update_border();
                        break;
                }
            } break;
            case 3:
                vdp_mode_select = 1;
                vdp_addr        = vdp_addr_latch & 0x1f;
                break;
        }
    }
}
void VDP::update_border()
{
    int64_t borderIndex = 16 + (vdp_regs[7] & 0xf);
    if (paletteRGB[borderIndex] == prev_border)
        return;
    prev_border = paletteRGB[borderIndex];
}
int64_t expandBits(int64_t val)
{
    int64_t v = val & 3;
    v |= v << 2;
    v |= v << 4;
    return v;
};
void VDP::vdp_writepalette(int64_t val)
{
    uint64_t valu = val;
    uint64_t r    = expandBits(valu);
    uint64_t g    = expandBits(valu >> 2);
    uint64_t b    = expandBits(valu >> 4);

    uint64_t pal_addr    = vdp_addr & 0x1f;
    paletteR[pal_addr]   = r;
    paletteG[pal_addr]   = g;
    paletteB[pal_addr]   = b;
    paletteRGB[pal_addr] = 0xff000000 | (r << 16) | (g << 8) | b;
    palette[pal_addr]    = val;
    vdp_addr             = (vdp_addr + 1) & 0x3fff;
    update_border();
}
void VDP::vdp_writeram(int64_t val)
{
    vram[vdp_addr]        = val;
    int64_t  planarBase   = vdp_addr & 0x3ffc;
    int64_t  twiddledBase = planarBase * 2;
    uint64_t val0         = vram[planarBase];
    uint64_t val1         = vram[planarBase + 1];
    uint64_t val2         = vram[planarBase + 2];
    uint64_t val3         = vram[planarBase + 3];

    for (int i = 0; i < 8; ++i) {
        int64_t effectiveBit = 7 - i;
        int64_t index        = ((val0 >> effectiveBit) & 1) | (((val1 >> effectiveBit) & 1) << 1) |
                        (((val2 >> effectiveBit) & 1) << 2) | (((val3 >> effectiveBit) & 1) << 3);
        vramUntwiddled[twiddledBase + i] = index;
    }
    vdp_addr = (vdp_addr + 1) & 0x3fff;
}
void VDP::vdp_writebyte(int64_t val)
{
    vdp_addr_state = 0;
    if (vdp_mode_select == 0) {
        vdp_writeram(val);
    } else {
        vdp_writepalette(val);
    }
}
uint64_t VDP::vdp_readram()
{
    int64_t res = vram[vdp_addr];
    vdp_addr    = (vdp_addr + 1) & 0x3fff;
    return res;
}
uint64_t VDP::vdp_readpalette()
{
    int64_t res = palette[vdp_addr & 0x1f];
    vdp_addr    = (vdp_addr + 1) & 0x3fff;
    return res;
}
uint64_t VDP::vdp_readbyte()
{
    vdp_addr_state = 0;
    if (vdp_mode_select == 0) {
        return vdp_readram();
    } else {
        return vdp_readpalette();
    }
}
uint64_t VDP::vdp_readstatus()
{
    int64_t res = vdp_status;
    vdp_status &= 0x1f;
    vdp_pending_hblank = false;
    core->cpu->z80_set_irq(0);
    vdp_addr_state = 0;
    return res;
}
int64_t *VDP::findSprites(int64_t line, int64_t *active)
{
    int64_t spriteInfo   = (vdp_regs[5] & 0x7e) << 7;
    int64_t actlen       = 0;
    int64_t spriteHeight = 8;
    int64_t i;

    if (vdp_regs[1] & 2) {
        spriteHeight = 16;
    }

    for (i = 0; i < 64; i++) {
        int64_t y = vram[spriteInfo + i];
        if (y == 208) {
            break;
        }
        if (y >= 240)
            y -= 256;
        if (line >= y && line < y + spriteHeight) {
            if (actlen == 8) {
                vdp_status |= 0x40;
                break;
            }
            // active.push([ vram[spriteInfo + 128 + i * 2], vram[spriteInfo + 128 + i * 2 + 1], y ]);

            actlen++;
        }
    }
    return active;
}
void VDP::rasterize_background(int64_t lineAddr, int64_t pixelOffset, int64_t tileData, int64_t tileDef,
                               int64_t transparent)
{
    lineAddr    = lineAddr | 0;
    pixelOffset = pixelOffset | 0;
    tileData    = tileData | 0;
    tileDef     = (tileDef | 0) * 2;

    int64_t i, tileDefInc;
    if (tileData & (1 << 9)) {
        tileDefInc = -1;
        tileDef += 7;
    } else {
        tileDefInc = 1;
    }

    int64_t paletteOffset = tileData & (1 << 11) ? 16 : 0;
    int64_t index;

    if (transparent && paletteOffset == 0) {
        for (i = 0; i < 8; i++) {
            index = vramUntwiddled[tileDef];
            tileDef += tileDefInc;
            if (index != 0)
                fb32[lineAddr + pixelOffset] = paletteRGB[index];
            pixelOffset = (pixelOffset + 1) & 255;
        }
    } else {
        index = vramUntwiddled[tileDef] + paletteOffset;
        tileDef += tileDefInc;
        fb32[lineAddr + pixelOffset] = paletteRGB[index];
        pixelOffset                  = (pixelOffset + 1) & 255;

        index = vramUntwiddled[tileDef] + paletteOffset;
        tileDef += tileDefInc;
        fb32[lineAddr + pixelOffset] = paletteRGB[index];
        pixelOffset                  = (pixelOffset + 1) & 255;

        index = vramUntwiddled[tileDef] + paletteOffset;
        tileDef += tileDefInc;
        fb32[lineAddr + pixelOffset] = paletteRGB[index];
        pixelOffset                  = (pixelOffset + 1) & 255;

        index = vramUntwiddled[tileDef] + paletteOffset;
        tileDef += tileDefInc;
        fb32[lineAddr + pixelOffset] = paletteRGB[index];
        pixelOffset                  = (pixelOffset + 1) & 255;

        index = vramUntwiddled[tileDef] + paletteOffset;
        tileDef += tileDefInc;
        fb32[lineAddr + pixelOffset] = paletteRGB[index];
        pixelOffset                  = (pixelOffset + 1) & 255;

        index = vramUntwiddled[tileDef] + paletteOffset;
        tileDef += tileDefInc;
        fb32[lineAddr + pixelOffset] = paletteRGB[index];
        pixelOffset                  = (pixelOffset + 1) & 255;

        index = vramUntwiddled[tileDef] + paletteOffset;
        tileDef += tileDefInc;
        fb32[lineAddr + pixelOffset] = paletteRGB[index];
        pixelOffset                  = (pixelOffset + 1) & 255;

        index                        = vramUntwiddled[tileDef] + paletteOffset;
        fb32[lineAddr + pixelOffset] = paletteRGB[index];
    }
}
void VDP::clear_background(int64_t lineAddr, int64_t pixelOffset)
{
    lineAddr    = lineAddr | 0;
    pixelOffset = pixelOffset | 0;

    int64_t rgb = paletteRGB[0];
    for (int i = 0; i < 8; ++i) {
        fb32[lineAddr + pixelOffset] = rgb;
        pixelOffset                  = (pixelOffset + 1) & 255;
    }
}
void VDP::rasterize_background_line(int64_t lineAddr, int64_t pixelOffset, int64_t nameAddr, int64_t yMod)
{
    lineAddr    = lineAddr | 0;
    pixelOffset = pixelOffset | 0;
    nameAddr    = nameAddr | 0;

    int64_t yOffset = (yMod | 0) * 4;

    for (int i = 0; i < 32; i++) {
        int64_t tileData = vram[nameAddr + i * 2] | (vram[nameAddr + i * 2 + 1] << 8);
        int64_t tileNum  = tileData & 511;
        int64_t tileDef  = 32 * tileNum;

        if (tileData & (1 << 10)) {
            tileDef += 28 - yOffset;
        } else {
            tileDef += yOffset;
        }

        if ((tileData & (1 << 12)) == 0) {
            rasterize_background(lineAddr, pixelOffset, tileData, tileDef, false);
        } else {
            clear_background(lineAddr, pixelOffset);
        }
        pixelOffset = (pixelOffset + 8) & 255;
    }
}
void VDP::rasterize_foreground_line(int64_t lineAddr, int64_t pixelOffset, int64_t nameAddr, int64_t yMod)
{
    lineAddr        = lineAddr | 0;
    pixelOffset     = pixelOffset | 0;
    nameAddr        = nameAddr | 0;
    int64_t yOffset = (yMod | 0) * 4;

    for (int i = 0; i < 32; i++) {
        int64_t tileData = vram[nameAddr + i * 2] | (vram[nameAddr + i * 2 + 1] << 8);
        if ((tileData & (1 << 12)) == 0)
            continue;

        int64_t tileNum = tileData & 511;
        int64_t tileDef = 32 * tileNum;
        if (tileData & (1 << 10)) {
            tileDef += 28 - yOffset;
        } else {
            tileDef += yOffset;
        }
        rasterize_background(lineAddr, (i * 8 + pixelOffset) & 0xff, tileData, tileDef, true);
    }
}
void VDP::rasterize_sprites(int64_t line, int64_t lineAddr, int64_t pixelOffset, int64_t *sprites)
{
    lineAddr           = lineAddr | 0;
    pixelOffset        = pixelOffset | 0;
    int64_t spriteBase = vdp_regs[6] & 4 ? 0x2000 : 0;

    for (int i = 0; i < 256; ++i) {
        int64_t xPos             = i;
        bool    spriteFoundThisX = false;
        bool    writtenTo        = false;
        int64_t minDistToNext    = 256;

        // for (int k = 0; k < sprites.length; k++) {
        //     int64_t sprite = sprites[k];
        //     int64_t offset = xPos - sprite[0];

        //     if (offset < 0) {
        //         int64_t distToSprite = -offset;

        //         if (distToSprite < minDistToNext)
        //             minDistToNext = distToSprite;
        //         continue;
        //     }
        //     if (offset >= 8)
        //         continue;

        //     spriteFoundThisX       = true;
        //     int64_t spriteLine     = line - sprite[2];
        //     int64_t spriteAddr     = spriteBase + sprite[1] * 32 + spriteLine * 4;
        //     int64_t untwiddledAddr = spriteAddr * 2 + offset;
        //     int64_t index          = vramUntwiddled[untwiddledAddr];

        //     if (index == 0) {
        //         continue;
        //     }
        //     if (writtenTo) {
        //         vdp_status |= 0x20;
        //         break;
        //     }
        //     // fb32[lineAddr + ((pixelOffset + i - vdp_regs[8]) & 0xff)] = paletteRGB[16 + index];
        //     writtenTo = true;
        // }
        if (!spriteFoundThisX && minDistToNext > 1) {
            i += minDistToNext - 1;
        }
    }
}
void VDP::border_clear(int64_t lineAddr, int64_t count)
{
    lineAddr            = lineAddr | 0;
    count               = count | 0;
    int64_t borderIndex = 16 + (vdp_regs[7] & 0xf);
    int64_t borderRGB   = paletteRGB[borderIndex];
    for (int i = 0; i < count; i++)
        fb32[lineAddr + i] = borderRGB;
}
void VDP::rasterize_line(int64_t line)
{
    line             = line | 0;
    int64_t lineAddr = (line * 256) | 0;

    if ((vdp_regs[1] & 64) == 0) {
        border_clear(lineAddr, 256);
        return;
    }

    int64_t effectiveLine = line + vdp_regs[9];
    if (effectiveLine >= 224) {
        effectiveLine -= 224;
    }

    uint64_t effectiveLineu = effectiveLine;
    // int64_t sprites     = findSprites(line);
    int64_t pixelOffset = vdp_regs[0] & 64 && line < 16 ? 0 : vdp_regs[8];
    int64_t nameAddr    = ((vdp_regs[2] << 10) & 0x3800) + (effectiveLineu >> 3) * 64;
    int64_t yMod        = effectiveLine & 7;

    rasterize_background_line(lineAddr, pixelOffset, nameAddr, yMod);
    // if (sprites.length)
    //     rasterize_sprites(line, lineAddr, pixelOffset, sprites);
    rasterize_foreground_line(lineAddr, pixelOffset, nameAddr, yMod);

    if (vdp_regs[0] & (1 << 5)) {
        border_clear(lineAddr, 8);
    }
}
uint64_t VDP::vdp_hblank()
{
    int64_t firstDisplayLine   = 3 + 13 + 54;
    int64_t pastEndDisplayLine = firstDisplayLine + 192;
    int64_t endOfFrame         = pastEndDisplayLine + 48 + 3;

    if (vdp_current_line == firstDisplayLine)
        vdp_hblank_counter = vdp_regs[10];
    if (vdp_current_line >= firstDisplayLine && vdp_current_line < pastEndDisplayLine) {
        rasterize_line(vdp_current_line - firstDisplayLine);
        if (--vdp_hblank_counter < 0) {
            vdp_hblank_counter = vdp_regs[10];
            vdp_pending_hblank = true;
        }
    }
    vdp_current_line++;
    int64_t needIrq = 0;

    if (vdp_current_line == endOfFrame) {
        vdp_current_line = 0;
        vdp_status |= 128;
        needIrq |= 4;
        currentFrame++;
    }

    if (vdp_regs[1] & 32 && vdp_status & 128) {
        needIrq |= 2;
    }
    if (vdp_regs[0] & 16 && vdp_pending_hblank) {
        needIrq |= 1;
    }
    return needIrq;
}
void VDP::vdp_reset()
{
    for (int i = 0x0000; i < 0x4000; i++) {
        vram[i] = 0;
    }
    for (int i = 0; i < 32; i++) {
        paletteR[i] = paletteG[i] = paletteB[i] = paletteRGB[i] = palette[i] = 0;
    }
    for (int i = 0; i < 16; i++) {
        vdp_regs[i] = 0;
    }
    for (int i = 2; i <= 5; i++) {
        vdp_regs[i] = 0xff;
    }

    vdp_regs[6]      = 0xfb;
    vdp_regs[10]     = 0xff;
    vdp_current_line = vdp_status = vdp_hblank_counter = 0;
    vdp_mode_select                                    = 0;
}
uint64_t VDP::vdp_get_line()
{
    return (vdp_current_line - 64) & 0xff;
}
