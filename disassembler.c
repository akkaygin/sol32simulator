#include <stdint.h>
#include <stdio.h>

uint32_t GetRegister(uint32_t Index);
void SetRegister(uint32_t Index, uint32_t Value);

uint32_t GetX(uint32_t Instruction);

uint32_t GetMemory(uint64_t Address, uint32_t Width);
void SetMemory(uint64_t Address, uint32_t Width, uint32_t Value);

uint32_t SignExtendXEmbedded(uint32_t Instruction);
uint32_t SignExtendOffset(uint32_t Instruction);

extern uint32_t RegisterSet[32];
extern uint32_t Flags;

/*    Disassembler    */
void DisassembleALInstruction(uint32_t Instruction)
{
  if(Instruction == 0 || Instruction == 0x80) { printf("noop\n"); return; }

  uint32_t Source2 = GetX(Instruction)? SignExtendXEmbedded(Instruction) : GetRegister((Instruction>>16)&15)+SignExtendOffset(Instruction);
  uint32_t Source1 = GetRegister((Instruction>>12)&15);
  uint32_t Result = 0;

  printf("R%d = R%d(%d) ", (Instruction>>8)&15, (Instruction>>12)&15, Source1);

  switch((Instruction>>4)&7)
  {
  case 0:
    Result = Source1+Source2;
    printf("+ ");
  break;

  case 1:
    Result = Source1-Source2;
    printf("- ");
  break;

  case 2:
    Result = Source1&Source2;
    printf("& ");
  break;

  case 3:
    Result = Source1|Source2;
    printf("| ");
  break;

  case 4:
    Result = Source1^Source2;
    printf("^ ");
  break;
  
  case 5:
    Result = Source1<<Source2;
    printf("<< ");
  break;

  case 6:
    Result = Source1>>Source2;
    printf(">> ");
  break;

  case 7:
    Result = (int)Source1>>Source2;
    printf("a> ");
  break;
  }

  if(GetX(Instruction)) {
    printf("%d = %d\n", Source2, Result);
  } else {
    printf("R%d + %d(%d) = %d\n", (Instruction>>16)&15, SignExtendOffset(Instruction), Source2, Result);
  }
}

void DisassembleLSInstruction(uint32_t Instruction)
{
  uint32_t Source2 = GetX(Instruction)? SignExtendXEmbedded(Instruction) : GetRegister((Instruction>>16)&15)+SignExtendOffset(Instruction);
  uint32_t Source1 = GetRegister((Instruction>>12)&15);
  uint64_t Address = (((uint64_t)Source1&0xFFFF)<<48)+(((uint64_t)Source1>>16)<<32);

  switch((Instruction>>4)&7)
  {
  case 0:
    Address += Source2<<2;
    printf("load word from %ld to R%d\n", Address, (Instruction>>8)&15);
  break;

  case 1:
    Address += Source2<<1;
    printf("load halfword from %ld to R%d\n", Address, (Instruction>>8)&15);
  break;

  case 2:
    printf("load byte from %ld to R%d\n", Address, (Instruction>>8)&15);
  break;

  case 4:
    Address += Source2<<2;
    printf("store word from R%d to %ld\n", (Instruction>>8)&15, Address);
  break;

  case 5:
    Address += Source2<<1;
    printf("store halfword from R%d to %ld\n", (Instruction>>8)&15, Address);
  break;

  case 6:
    printf("store byte from R%d to %ld\n", (Instruction>>8)&15, Address);
  break;
  }
}

void DisassembleCJInstruction(uint32_t Instruction)
{
  uint32_t Compare2 = GetRegister((Instruction>>8)&15);
  uint32_t Compare1 = GetRegister((Instruction>>12)&3151);
  uint32_t Source2 = GetX(Instruction)? SignExtendXEmbedded(Instruction) : GetRegister((Instruction>>16)&15)+SignExtendOffset(Instruction);

  switch((Instruction>>4)&7)
  {
  case 0:
    printf("goto R15(%X)+%X=%X if R%d(%d) == R%d(%d)",GetRegister(15)<<2, Source2<<2, (GetRegister(15)<<2)+(Source2<<2), (Instruction>>12)&15, Compare1, (Instruction>>8)&15, Compare2);
    if(Compare1 == Compare2) { printf(" (true)\n"); } else { printf(" (false)\n"); }
  break;

  case 1:
    printf("goto R15(%X)+%X=%X if R%d(%d) != R%d(%d)", GetRegister(15)<<2, Source2<<2, (GetRegister(15)<<2)+(Source2<<2), (Instruction>>12)&15, Compare1, (Instruction>>8)&15, Compare2);
    if(Compare1 != Compare2) { printf(" (true)\n"); } else { printf(" (false)\n"); }
  break;
  
  case 2:
    printf("goto R15(%X)+%X=%X if R%d(%d) s< R%d(%d)", GetRegister(15)<<2, Source2<<2, (GetRegister(15)<<2)+(Source2<<2), (Instruction>>12)&15, Compare1, (Instruction>>8)&15, Compare2);
    if((int32_t)Compare1 < (int32_t)Compare2) { printf(" (true)\n"); } else { printf(" (false)\n"); }
  break;

  case 3:
    printf("goto R15(%X)+%X=%X if R%d(%d) < R%d(%d)", GetRegister(15)<<2, Source2<<2, (GetRegister(15)<<2)+(Source2<<2), (Instruction>>12)&15, Compare1, (Instruction>>8)&15, Compare2);
    if(Compare1 < Compare2) { printf(" (true)\n"); } else { printf(" (false)\n"); }
  break;

  case 4:
    printf("goto R15(%X)+%X=%X if R%d(%d) s<= R%d(%d)", GetRegister(15)<<2, Source2<<2, (GetRegister(15)<<2)+(Source2<<2), (Instruction>>12)&15, Compare1, (Instruction>>8)&15, Compare2);
    if((int32_t)Compare1 <= (int32_t)Compare2) { printf(" (true)\n"); } else { printf(" (false)\n"); }
  break;

  case 5:
    printf("goto R15(%X)+%X=%X if R%d(%d) <= R%d(%d)", GetRegister(15)<<2, Source2<<2, (GetRegister(15)<<2)+(Source2<<2), (Instruction>>12)&15, Compare1, (Instruction>>8)&15, Compare2);
    if(Compare1 <= Compare2) { printf(" (true)\n"); } else { printf(" (false)\n"); }
  break;

  case 6:
    printf("goto R15(%X)+%X=%X if R%d(%d) s>= R%d(%d)", GetRegister(15)<<2, Source2<<2, (GetRegister(15)<<2)+(Source2<<2), (Instruction>>12)&15, Compare1, (Instruction>>8)&15, Compare2);
    if((int32_t)Compare1 >= (int32_t)Compare2) { printf(" (true)\n"); } else { printf(" (false)\n"); }
  break;

  case 7:
    printf("goto R15(%X)+%X=%X if R%d(%d) >= R%d(%d)", GetRegister(15)<<2, Source2<<2, (GetRegister(15)<<2)+(Source2<<2), (Instruction>>12)&15, Compare1, (Instruction>>8)&15, Compare2);
    if(Compare1 >= Compare2) { printf(" (true)\n"); } else { printf(" (false)\n"); }
  break;
  }
}

void DisassembleCCInstruction(uint32_t Instruction)
{
  switch((Instruction>>4)&27)
  {
  case 0:
    Flags>>31? printf("SVM handoff\n") : printf("USM handoff\n");
  break;

  case 1:
    printf("SR%d(%d)->UR%d(%d)\n", (Instruction>>16)&15, RegisterSet[0x10|(Instruction>>16)&15], (Instruction>>12)&15, RegisterSet[(Instruction>>12)&15]);
  break;

  case 2:
    printf("UR%d(%d)->SR%d(%d)\n", (Instruction>>12)&15, RegisterSet[(Instruction>>12)&15], (Instruction>>16)&15, RegisterSet[0x10|(Instruction>>16)&15]);
  break;

  case 3:
    printf("Halt Core\n");
  break;
  }
}

void Disassemble(uint32_t Instruction, uint64_t Address)
{
  printf("%X| ", Address);

  switch (Instruction&3)
  {
  case 0:
    DisassembleALInstruction(Instruction);
  break;

  case 1:
    DisassembleLSInstruction(Instruction);
  break;

  case 2:
    DisassembleCJInstruction(Instruction);
  break;

  case 3:
    DisassembleCCInstruction(Instruction);
  break;
  }
}