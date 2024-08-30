#include "CodeGen.h"

////////////////////////////////////////
#pragma mark - IR Visitor
////////////////////////////////////////

void ToIRVisitor::visit(Factor &Node) {
  if (Node.getKind() == Factor::Ident) {
    V = nameMap[Node.getVal()];
  } else {
    int intVal;
    Node.getVal().getAsInteger(10, intVal);
    V = llvm::ConstantInt::get(Int32Type, intVal, true);
  }
}

void ToIRVisitor::visit(BinaryOp &Node) {
  // get left and right operands
  Node.getLeft()->accept(*this);
  llvm::Value *Left = V;
  Node.getRight()->accept(*this);
  llvm::Value *Right = V;

  switch (Node.getOperator()) {
  case BinaryOp::Plus:
    V = Builder.CreateNSWAdd(Left, Right);
    break;
  case BinaryOp::Minus:
    V = Builder.CreateNSWSub(Left, Right);
    break;
  case BinaryOp::Mul:
    V = Builder.CreateNSWMul(Left, Right);
    break;
  case BinaryOp::Div:
    V = Builder.CreateSDiv(Left, Right);
    break;
  }
}

void ToIRVisitor::visit(With &Node) {
  llvm::FunctionType *ReadFnType =
      llvm::FunctionType::get(Int32Type, {PtrType}, false);
  llvm::Function *ReadFn = llvm::Function::Create(
      ReadFnType, llvm::GlobalValue::ExternalLinkage, "calc_read", M);

  for (auto it = Node.begin(); it != Node.end(); it++) {
    llvm::StringRef Curr = *it; // remember an array of SmallVec's with 8 len

    llvm::Constant *StrText =
        llvm::ConstantDataArray::getString(M->getContext(), Curr);
    llvm::GlobalVariable *Str = new llvm::GlobalVariable(
        *M, StrText->getType(), true, llvm::GlobalValue::PrivateLinkage,
        StrText, llvm::Twine(Curr).concat(".str"));


    llvm::CallInst* Call = Builder.CreateCall(ReadFnType, ReadFn, Str);
    nameMap[Curr] = Call;
  }

  Node.getExpr()->accept(*this);
}


void ToIRVisitor::run(AST* Tree) {
    llvm::FunctionType *MainFnType = llvm::FunctionType::get(Int32Type, {Int32Type, PtrType}, false);
    llvm::Function *MainFn = llvm::Function::Create(MainFnType, llvm::GlobalValue::ExternalLinkage, "main", M);

    llvm::BasicBlock* Blk = llvm::BasicBlock::Create(M->getContext(), "entry", MainFn);
    Builder.SetInsertPoint(Blk); // specifies that all instructions added to build should be appended inside this block(main)

    Tree->accept(*this);

    llvm::FunctionType* CalcWriteFnType = llvm::FunctionType::get(VoidType, {Int32Type}, false);
    llvm::Function* CalcWriteFn = llvm::Function::Create(CalcWriteFnType, llvm::GlobalValue::ExternalLinkage, "calc_write", M);

    Builder.CreateCall(CalcWriteFnType, CalcWriteFn, {V});
    Builder.CreateRet(Int32Zero);
}

////////////////////////////////////////
#pragma mark - CodeGen
////////////////////////////////////////

bool CodeGen::compile(AST *Tree) {
    // LLVM init
    llvm::LLVMContext Ctx;
    llvm::Module *M = new llvm::Module("calc.expr", Ctx);

    // Our init+Run
    ToIRVisitor Visitor(M);
    Visitor.run(Tree);
    M->print(llvm::outs(), nullptr);

    // later make this a check to check if any of the LLVM IR Builders encountered errors
    return true;
}
