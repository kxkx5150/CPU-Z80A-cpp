#ifndef _HrPC
#define _HrPC

#include "mem.h"
#include "z80.h"
#include "vdp.h"
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstddef>

class PC {
  public:
    PC();
    ~PC();

    void init();
    void load(std::string path);
    void start();
    void run_cpu();

  public:
    Mem *mem = nullptr;
    z80 *cpu = nullptr;
    VDP *vdp = nullptr;

  private:
    int *bin1 = nullptr;
    int *bin2 = nullptr;

    int mem_size   = 16 * 1024 * 1024;
    int start_addr = 0x10000;
    int steps      = -1;

  public:
    int fps           = 60;
    int cpuHz         = 3.58 * 1000 * 1000;
    int targetTimeout = 1000 / fps;

    int64_t tstates          = 0;
    int64_t event_next_event = 191;
    bool    imgok            = false;
};
#endif
