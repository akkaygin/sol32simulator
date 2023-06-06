#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>

void Disassemble(uint32_t Instruction, uint64_t Address);

/*    Simulator    */
#define MEMORY_SIZE 4
uint8_t*Memory;

uint32_t RegisterSet[32] = { 0 };
uint32_t Flags = 0x80000000;
uint32_t Halted = 0; // move to Flags??

uint32_t GetRegister(uint32_t Index)
{
  return RegisterSet[Flags>>27|(Index&31)];
}

void SetRegister(uint32_t Index, uint32_t Value)
{
  if(Index == 0) { return; }
  RegisterSet[(Flags>>27)|(Index&31)] = Value;
}

uint32_t GetX(uint32_t Instruction)
{
  return (Instruction >> 7)&1;
}

typedef struct
{
  uint64_t VirtualBase;
  uint64_t RealBase;
  uint64_t Range;
} rangetableentry;

typedef struct
{
  rangetableentry*CurrentBlock;
  uint64_t BlockLength;
} s32MMU;
s32MMU MMU;

uint32_t CheckAccess(uint64_t Address, uint32_t Width)
{
  if(Flags >> 31) { return 1; }
  for(uint64_t i = 0; i < MMU.BlockLength; i++) { // should work but might not :o
    if(MMU.CurrentBlock[i].VirtualBase < Address
    && Address < MMU.CurrentBlock[i].VirtualBase + MMU.CurrentBlock[i].Range
    && MMU.CurrentBlock[i].VirtualBase < Address + (2-Width)
    && Address + (2-Width) < MMU.CurrentBlock[i].VirtualBase + MMU.CurrentBlock[i].Range) {
      return 1;
    }
  } return 0;
}

uint32_t GetMemory(uint64_t Address, uint32_t Width)
{
  if(!CheckAccess(Address, Width)) { return 0; }
  switch(Address >> 48) {
  case 0:
    switch(Width)
    {
    case 0:
      return (Memory[(Address+3)&0xFFFFFFFFFFFFFFFF]<<24)
           | (Memory[(Address+2)&0xFFFFFFFFFFFFFFFF]<<16)
           | (Memory[(Address+1)&0xFFFFFFFFFFFFFFFF]<<8)
           |  Memory[(Address)&0xFFFFFFFFFFFFFFFF];
    break;
    
    case 1:
      return (Memory[(Address+1)&0xFFFFFFFFFFFFFFFF]<<16)
           |  Memory[(Address)&0xFFFFFFFFFFFF];
    break;
    
    case 2:
      return Memory[(Address)&0xFFFFFFFFFFFF];
    break;
    }
  break;

  default: return 0;
  }
}

void SetMemory(uint64_t Address, uint32_t Width, uint32_t Value)
{
  if(!CheckAccess(Address, Width)) { return; }
  switch(Address >> 48) {
  case 0:
    switch(Width)
    {
    case 0:
      Memory[(Address+3)&0xFFFFFFFFFFFF] = (Value >> 24) & 0xFF;
      Memory[(Address+2)&0xFFFFFFFFFFFF] = (Value >> 16) & 0xFF;
      Memory[(Address+1)&0xFFFFFFFFFFFF] = (Value >> 8) & 0xFF;
      Memory[(Address)&0xFFFFFFFFFFFF] = Value & 0xFF;
    break;
    
    case 1:
      Memory[(Address+1)&0xFFFFFFFFFFFF] = (Value >> 8) & 0xFF;
      Memory[(Address)&0xFFFFFFFFFFFF] = Value & 0xFF;
    break;
    
    case 2:
      Memory[Address&0xFFFFFFFFFFFF] = Value & 0xFF;
    break;
    }
  break;
  }
}

uint32_t SignExtendXEmbedded(uint32_t Instruction)
{
  return Instruction>>31? (-1<<16)|(Instruction>>16) : (Instruction>>16);
}

uint32_t SignExtendOffset(uint32_t Instruction)
{
  return Instruction>>31? (-1<<12)|(Instruction>>20) : (Instruction>>20);
}

void SimulateALInstruction(uint32_t Instruction)
{
  uint32_t Source2 = GetX(Instruction)? SignExtendXEmbedded(Instruction) : GetRegister((Instruction>>16)&15)+SignExtendOffset(Instruction);
  uint32_t Source1 = GetRegister((Instruction>>12)&15);
  uint32_t Result = 0;

  switch((Instruction>>4)&7)
  {
  case 0:
    Result = Source1+Source2;
  break;

  case 1:
    Result = Source1-Source2;
  break;

  case 2:
    Result = Source1&Source2;
  break;

  case 3:
    Result = Source1|Source2;
  break;

  case 4:
    Result = Source1^Source2;
  break;
  
  case 5:
    Result = Source1<<Source2;
  break;

  case 6:
    Result = Source1>>Source2;
  break;

  case 7:
    Result = (int)Source1>>Source2;
  break;
  }

  SetRegister((Instruction>>8)&15, Result);
}

void SimulateLSInstruction(uint32_t Instruction)
{
  uint32_t Source2 = GetX(Instruction)? SignExtendXEmbedded(Instruction) : GetRegister((Instruction>>16)&15)+SignExtendOffset(Instruction);
  uint32_t Source1 = GetRegister((Instruction>>12)&15);
  uint64_t Address = (((uint64_t)Source1&0xFFFF)<<48)+(((uint64_t)Source1>>16)<<32);

  switch((Instruction>>4)&7)
  {
  case 0:
    Address += (uint64_t)Source2<<2;
    SetRegister((Instruction>>8)&15, GetMemory(Address, 0));
  break;

  case 1:
    Address += (uint64_t)Source2<<1;
    SetRegister((Instruction>>8)&15, GetMemory(Address, 1));
  break;

  case 2:
    SetRegister((Instruction>>8)&15, GetMemory(Address, 2));
  break;

  case 4:
    Address += (uint64_t)Source2<<2;
    SetMemory(Address, 0, GetRegister((Instruction>>8)&15));
  break;

  case 5:
    Address += (uint64_t)Source2<<1;
    SetMemory(Address, 1, GetRegister((Instruction>>8)&15));
  break;

  case 6:
    SetMemory(Address, 2, GetRegister((Instruction>>8)&15));
  break;
  }
}

void SimulateCJInstruction(uint32_t Instruction)
{
  uint32_t Compare2 = GetRegister((Instruction>>8)&15);
  uint32_t Compare1 = GetRegister((Instruction>>12)&15);
  uint32_t Source2 = (GetX(Instruction)? SignExtendXEmbedded(Instruction) : GetRegister((Instruction>>16)&15)+SignExtendOffset(Instruction))-1;

  switch((Instruction>>4)&2)
  {
  case 0:
    if(Compare1 == Compare2) { SetRegister(15, GetRegister(15)+Source2); }
  break;

  case 1:
    if(Compare1 != Compare2) { SetRegister(15, GetRegister(15)+Source2); }
  break;
  
  case 2:
    if((int32_t)Compare1 < (int32_t)Compare2) { SetRegister(15, GetRegister(15)+Source2); }
  break;

  case 3:
    if(Compare1 < Compare2) { SetRegister(15, GetRegister(15)+Source2); }
  break;

  case 4:
    if((int32_t)Compare1 <= (int32_t)Compare2) { SetRegister(15, GetRegister(15)+Source2); }
  break;

  case 5:
    if(Compare1 <= Compare2) { SetRegister(15, GetRegister(15)+Source2); }
  break;

  case 6:
    if((int32_t)Compare1 >= (int32_t)Compare2) { SetRegister(15, GetRegister(15)+Source2); }
  break;

  case 7:
    if(Compare1 >= Compare2) { SetRegister(15, GetRegister(15)+Source2); }
  break;
  }
}

void SimulateCCInstruction(uint32_t Instruction)
{
  switch((Instruction>>4)&27)
  {
  case 0:
    Flags ^= 0x80000000;
  break;

  case 1:
    RegisterSet[0x10|(Instruction>>16)&15] = RegisterSet[(Instruction>>12)&15];
  break;

  case 2:
    RegisterSet[(Instruction>>12)&15] = RegisterSet[0x10|(Instruction>>16)&15];
  break;

  case 3:
    Halted ^= 1;
  break;
  }
}

void SimulatorMain(uint32_t Instruction)
{
  switch (Instruction&3)
  {
  case 0:
    SimulateALInstruction(Instruction);
  break;

  case 1:
    SimulateLSInstruction(Instruction);
  break;

  case 2:
    SimulateCJInstruction(Instruction);
  break;

  case 3:
    SimulateCCInstruction(Instruction);
  break;
  }
}

int main(int argc, char**argv) {
  Memory = (uint8_t*)malloc(MEMORY_SIZE*sizeof(uint8_t));
  memset(Memory, 0, MEMORY_SIZE*sizeof(uint8_t));
  memset(RegisterSet, 0, 32*sizeof(uint32_t));

  if(argc == 2) {
    FILE*FileHandle = fopen(argv[1], "rb");
    if(FileHandle == NULL) {
      printf("Error opening file\n");
      return 1;
    }
    fseek(FileHandle, 0, SEEK_END);
    int FileLength = ftell(FileHandle);
    rewind(FileHandle);
    fread(Memory, sizeof(uint8_t), FileLength, FileHandle);
    fclose(FileHandle);
  }

  while(!Halted && GetRegister(15) < MEMORY_SIZE) {
    Disassemble(GetMemory(GetRegister(15)<<2, 0), GetRegister(15)<<2);
    SimulatorMain(GetMemory(GetRegister(15)<<2, 0));
    SetRegister(15, GetRegister(15)+1);
  }
  
  return 0;
}