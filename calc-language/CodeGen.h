#pragma once

#include "AST.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

class CodeGen {
public:
  bool compile(AST *Tree);
};

/// @class LLVMContext
/// @brief Used down below when calling ->getContext() to initate the
/// llvm::Module This is an important class for using LLVM in a threaded
/// context.  It (opaquely) owns and manages the core "global" data of LLVM's
/// core infrastructure, including the type and constant uniquing tables.
/// LLVMContext itself provides no locking guarantees, so you should be careful
/// to have one context per thread.

class ToIRVisitor : public ASTVisitor {
public:
  ToIRVisitor(llvm::Module *M) : M(M), Builder(M->getContext()) {
    VoidType = llvm::Type::getVoidTy(M->getContext());
    Int32Type = llvm::Type::getInt32Ty(M->getContext());
    PtrType = llvm::PointerType::getUnqual(M->getContext()); // opaque ptr
    Int32Zero = llvm::ConstantInt::get(Int32Type, 0, true);
  }

  void run(AST *Tree);

  // Override visitors (impl)
  virtual void visit(Factor &Node) override;
  virtual void visit(BinaryOp &Node) override;
  virtual void visit(With &Node) override;

private:
  llvm::Module *M; // entire compilation stored in module
  llvm::IRBuilder<> Builder;

  // Representing Types in LLVM IR
  // To avoid repeated lookups, we cache the needed type instances: VoidTy,
  // Int32Ty, PtrTy, and Int32Zero
  llvm::Type *VoidType, *Int32Type;
  llvm::PointerType *PtrType;

  llvm::Constant *Int32Zero;
  llvm::Value *V;
  llvm::StringMap<llvm::Value *> nameMap;
};