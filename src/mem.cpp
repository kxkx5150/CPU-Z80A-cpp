#include "mem.h"
#include <cstdint>

Mem::Mem(PC *_pc)
{
    reset();
}
Mem::~Mem()
{
}
void Mem::reset()
{
    for (uint64_t j = 0; j < 0x2000; j++) {
        ram[j] = 0;
    }
    for (uint64_t j = 0; j < 0x8000; j++) {
        cartridgeRam[j] = 0;
    }
}
void Mem::load(uint8_t *buffer, int len)
{
    uint64_t numRomBanks = len / 0x4000;
    for (uint64_t i = 0; i < numRomBanks; i++) {
        romBanks[i] = new uint8_t[0x4000]{};
        for (uint64_t j = 0; j < 0x4000; j++) {
            romBanks[i][j] = buffer[i * 0x4000 + j];
        }
    }

    for (int i = 0; i < 3; i++) {
        pages[i] = i % numRomBanks;
    }
    romPageMask = (numRomBanks - 1) | 0;
}
int Mem::readMem(int address)
{
    address           = address | 0;
    uint64_t addressu = address;
    int      page     = (addressu >> 14) & 3;
    address &= 0x3fff;

    switch (page) {
        case 0:
            if (address < 0x0400) {
                return romBanks[0][address];
            }
            return romBanks[pages[0]][address];
        case 1:
            return romBanks[pages[1]][address];
        case 2:
            switch (ramSelectRegister & 12) {
                default:
                    break;
                case 8:
                    return cartridgeRam[address];
                case 12:
                    return cartridgeRam[address + 0x4000];
            }
            return romBanks[pages[2]][address];
        case 3:
            return ram[address & 0x1fff];
    }
    return 0;
}
void Mem::writeMem(int address, int value)
{
    address = address | 0;
    value   = value | 0;
    if (address >= 0xfffc) {
        switch (address) {
            case 0xfffc:
                ramSelectRegister = value;
                break;
            case 0xfffd:
                value &= romPageMask;
                pages[0] = value;
                break;
            case 0xfffe:
                value &= romPageMask;
                pages[1] = value;
                break;
            case 0xffff:
                value &= romPageMask;
                pages[2] = value;
                break;
            default:
                throw "zoiks";
        }
    }
    address -= 0xc000;
    if (address < 0) {
        return;
    }
    ram[address & 0x1fff] = value;
}
int Mem::readMemWord(int address)
{
    uint64_t hval = readMem(address + 1);
    uint64_t lval = readMem(address);
    return (hval << 8) | lval;
}
void Mem::writeMemWord(int address, int value)
{
    writeMem(address, value & 0xff);
    writeMem(address + 1, value >> 8);
}
void Mem::preparaMemoriaMegarom(std::string str)
{
}
