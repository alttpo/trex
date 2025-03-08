
const char *opcode_names[] = {
    "halt", "ret",
    "imm8", "imm16", "imm24", "imm32",
    "psh",  "pop",
    "bz",   "bnz",
    "ldloc","stloc",
    "setst",
    "or",   "xor",   "and",
    "eq",   "ne",    "ltu",   "lts",   "gtu",   "gts",   "leu",   "les",   "geu",   "ges",
    "shl",  "shru",  "shrs",
    "add",  "sub",   "mul",
    "sysc"
};

const uint8_t opcode_immediate_bytes[] = {
  //HALT, RET,
    0,    0,
  //IMM8, IMM16, IMM24, IMM32,
    1,    2,     3,     4,
  //PSH,  POP,
    0,    0,
  //BZ,   BNZ,
    1,    1,
  //LDLOC,STLOC,
    1,    1,
  //SETST,
    2,
  //OR,   XOR,   AND,
    0,    0,     0,
  //EQ,   NE,    LTU,   LTS,   GTU,   GTS,   LEU,   LES,   GEU,   GES,
    0,    0,     0,     0,     0,     0,     0,     0,     0,     0,
  //SHL,  SHRU,  SHRS,
    0,    0,     0,
  //ADD,  SUB,   MUL,
    0,    0,     0,
  //SYSC,
    1,
};
