#include "PC.h"
#include <cstdint>
#include <cstdio>
#include <thread>
#include <chrono>

PC::PC()
{
    mem = new Mem(this);
    cpu = new z80(this, mem);
    vdp = new VDP(this);
}
PC::~PC()
{
    delete cpu;
    delete mem;
    delete vdp;
}
void PC::init()
{
    printf("load file\n");
    load("64 Color Palette Test (PD).sms");
}
void PC::load(std::string path)
{
    FILE *f = fopen(path.c_str(), "rb");
    fseek(f, 0, SEEK_END);
    const int size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buffer = new uint8_t[size];
    auto     _      = fread(buffer, size, 1, f);
    mem->load(buffer, size);
    fclose(f);
}
void PC::start()
{
}
void PC::run_cpu()
{
    tstates -= event_next_event;
    while (tstates < event_next_event) {
        cpu->run();
    }
    int vdp_status = vdp->vdp_hblank();
    if (vdp_status & 4) {
        cpu->z80_set_irq(vdp_status & 3);
        imgok = true;
    }
}
