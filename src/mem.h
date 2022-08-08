#ifndef _H_MEM_
#define _H_MEM_
#include <string>
#include <vector>

class PC;
class Mem {
  public:
    PC *pc = nullptr;

    int ram[0x2000]{};
    int cartridgeRam[0x8000]{};

    uint8_t *romBanks[64];
    int      pages[3];
    uint64_t romPageMask       = 0;
    int      ramSelectRegister = 0;

    bool megarom       = false;
    int  pagMegaRom[4] = {0, 1, 2, 3};
    int  tipoMegarom   = 0;
    int  cartSlot      = 0;

    int PPIPortA = 0;
    int PPIPortC = 0;
    int PPIPortD = 0;

    bool podeEscrever[4] = {false, false, false, true};

  public:
    Mem(PC *pc);
    ~Mem();

    void load(uint8_t *buffer, int len);

    void reset();
    int  readMem(int address);
    int  readMemWord(int address);
    void writeMem(int address, int value);
    void writeMemWord(int address, int value);
    void preparaMemoriaMegarom(std::string address);
};

#endif