#include <stdio.h>
#include "NDS.h"
#include "ARM.h"
#include "ARMInterpreter.h"


u32 ARM::ConditionTable[16] =
{
    0xF0F0, // EQ
    0x0F0F, // NE
    0xCCCC, // CS
    0x3333, // CC
    0xFF00, // MI
    0x00FF, // PL
    0xAAAA, // VS
    0x5555, // VC
    0x0C0C, // HI
    0xF3F3, // LS
    0xAA55, // GE
    0x55AA, // LT
    0x0A05, // GT
    0xF5FA, // LE
    0xFFFF, // AL
    0x0000  // NE
};


ARM::ARM(u32 num)
{
    // well uh
    Num = num;
}

ARM::~ARM()
{
    // dorp
}

void ARM::Reset()
{
    for (int i = 0; i < 16; i++)
        R[i] = 0;

    CPSR = 0x000000D3;

    ExceptionBase = Num ? 0x00000000 : 0xFFFF0000;

    // zorp
    JumpTo(ExceptionBase);
}

void ARM::JumpTo(u32 addr)
{
    // pipeline shit

    //printf("jump from %08X to %08X\n", R[15] - ((CPSR&0x20)?4:8), addr);

    if (addr&1)
    {
        addr &= ~1;
        NextInstr = Read16(addr);
        R[15] = addr+2;
        CPSR |= 0x20;
    }
    else
    {
        addr &= ~3;
        NextInstr = Read32(addr);
        R[15] = addr+4;
        CPSR &= ~0x20;
    }
}

void ARM::RestoreCPSR()
{
    switch (CPSR & 0x1F)
    {
    case 0x11:
        CPSR = R_FIQ[8];
        break;

    case 0x12:
        CPSR = R_IRQ[2];
        break;

    case 0x13:
        CPSR = R_SVC[2];
        break;

    case 0x17:
        CPSR = R_ABT[2];
        break;

    case 0x1B:
        CPSR = R_UND[2];
        break;

    default:
        printf("!! attempt to restore CPSR under bad mode %02X\n", CPSR&0x1F);
        break;
    }
}

void ARM::UpdateMode(u32 oldmode, u32 newmode)
{
    u32 temp;
    #define SWAP(a, b)  temp = a; a = b; b = temp;

    if ((oldmode & 0x1F) == (newmode & 0x1F)) return;

    switch (oldmode & 0x1F)
    {
    case 0x11:
        SWAP(R[8], R_FIQ[0]);
        SWAP(R[9], R_FIQ[1]);
        SWAP(R[10], R_FIQ[2]);
        SWAP(R[11], R_FIQ[3]);
        SWAP(R[12], R_FIQ[4]);
        SWAP(R[13], R_FIQ[5]);
        SWAP(R[14], R_FIQ[6]);
        break;

    case 0x12:
        SWAP(R[13], R_IRQ[0]);
        SWAP(R[14], R_IRQ[1]);
        break;

    case 0x13:
        SWAP(R[13], R_SVC[0]);
        SWAP(R[14], R_SVC[1]);
        break;

    case 0x17:
        SWAP(R[13], R_ABT[0]);
        SWAP(R[14], R_ABT[1]);
        break;

    case 0x1B:
        SWAP(R[13], R_UND[0]);
        SWAP(R[14], R_UND[1]);
        break;
    }

    switch (newmode & 0x1F)
    {
    case 0x11:
        SWAP(R[8], R_FIQ[0]);
        SWAP(R[9], R_FIQ[1]);
        SWAP(R[10], R_FIQ[2]);
        SWAP(R[11], R_FIQ[3]);
        SWAP(R[12], R_FIQ[4]);
        SWAP(R[13], R_FIQ[5]);
        SWAP(R[14], R_FIQ[6]);
        break;

    case 0x12:
        SWAP(R[13], R_IRQ[0]);
        SWAP(R[14], R_IRQ[1]);
        break;

    case 0x13:
        SWAP(R[13], R_SVC[0]);
        SWAP(R[14], R_SVC[1]);
        break;

    case 0x17:
        SWAP(R[13], R_ABT[0]);
        SWAP(R[14], R_ABT[1]);
        break;

    case 0x1B:
        SWAP(R[13], R_UND[0]);
        SWAP(R[14], R_UND[1]);
        break;
    }

    #undef SWAP
}

s32 ARM::Execute(s32 cycles)
{
    while (cycles > 0)
    {
        if (CPSR & 0x20) // THUMB
        {
            // prefetch
            CurInstr = NextInstr;
            NextInstr = Read16(R[15]);
            R[15] += 2;

            // actually execute
            u32 icode = (CurInstr >> 6);
            cycles -= ARMInterpreter::THUMBInstrTable[icode](this);
        }
        else
        {
            // prefetch
            CurInstr = NextInstr;
            NextInstr = Read32(R[15]);
            R[15] += 4;

            // actually execute
            if (CheckCondition(CurInstr >> 28))
            {
                u32 icode = ((CurInstr >> 4) & 0xF) | ((CurInstr >> 16) & 0xFF0);
                cycles -= ARMInterpreter::ARMInstrTable[icode](this);
            }
            else if ((CurInstr & 0xFE000000) == 0xFA000000)
            {
                cycles -= ARMInterpreter::A_BLX_IMM(this);
            }
            else
            {
                // not executing it. oh well
                cycles -= 1; // 1S. todo: check
            }
        }
    }

    return cycles;
}