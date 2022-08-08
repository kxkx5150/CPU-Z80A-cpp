#ifndef _H_VDP_
#define _H_VDP_
#include <cstdint>

class PC;
class VDP {
  public:
    PC *core = nullptr;

    int64_t  vram[0x4000];
    int64_t  vramUntwiddled[0x8000];
    uint32_t fb32[256 * 192]{};

    int64_t vdp_regs[16];
    int64_t palette[32];
    int64_t paletteR[32];
    int64_t paletteG[32];
    int64_t paletteB[32];
    int64_t paletteRGB[32];

    int64_t currentFrame       = 1;
    int64_t vdp_addr_state     = 0;
    int64_t vdp_mode_select    = 0;
    int64_t vdp_addr_latch     = 0;
    int64_t vdp_addr           = 0;
    int64_t vdp_current_line   = 0;
    int64_t vdp_status         = 0;
    bool    vdp_pending_hblank = false;
    int64_t vdp_hblank_counter = 0;
    int64_t prev_border        = 0;

  public:
    VDP(PC *_core);

    void     vdp_writeaddr(int64_t val);
    void     update_border();
    void     vdp_writepalette(int64_t val);
    void     vdp_writeram(int64_t val);
    void     vdp_writebyte(int64_t val);
    uint64_t vdp_readram();
    uint64_t vdp_readpalette();
    uint64_t vdp_readbyte();
    uint64_t vdp_readstatus();
    int64_t *findSprites(int64_t line, int64_t *active);
    void     rasterize_background(int64_t lineAddr, int64_t pixelOffset, int64_t tileData, int64_t tileDef,
                                  int64_t transparent);
    void     clear_background(int64_t lineAddr, int64_t pixelOffset);
    void     rasterize_background_line(int64_t lineAddr, int64_t pixelOffset, int64_t nameAddr, int64_t yMod);
    void     rasterize_foreground_line(int64_t lineAddr, int64_t pixelOffset, int64_t nameAddr, int64_t yMod);
    void     rasterize_sprites(int64_t line, int64_t lineAddr, int64_t pixelOffset, int64_t *sprites);
    void     border_clear(int64_t lineAddr, int64_t count);
    void     rasterize_line(int64_t line);
    uint64_t vdp_hblank();
    void     vdp_reset();
    uint64_t vdp_get_line();

  private:
};
#endif
