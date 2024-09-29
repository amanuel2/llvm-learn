class Inst<string mnemonic, int opcode> {
  string Mnemonic = mnemonic;
  int Opcode = opcode;
  bit Commutable = 0;
}

class InstDesc<string name, string suffix, int delta> {
  string Name = name;
  string Suffix = suffix;
  int Delta = delta;
}

def RegOp : InstDesc<"",  "", 0>;
def ImmOp : InstDesc<"I", "", 1>;
def MemOp : InstDesc<"M", "m", 2>;

class getDesc<int n> {
  InstDesc ret = !if(!eq(n, 0), RegOp,
                 !if(!eq(n, 1), ImmOp,
                 !if(!eq(n, 2), MemOp, ?)));
}

multiclass InstWithOps<string mnemonic, int opcode> {
  foreach I = [0, 1, 2] in {
    def NAME#!if(!eq(I, 1), "I", !if(!eq(I, 2), "M", "")) :
      Inst<mnemonic # !if(!eq(I, 2), "m", ""),
           !add(opcode, I)> {
      let Commutable = !if(!eq(I, 0), 1, 0);
    }
  }
}

defm ADD : InstWithOps<"add", 0xA0>;
defm SUB : InstWithOps<"sub", 0xB0>;
