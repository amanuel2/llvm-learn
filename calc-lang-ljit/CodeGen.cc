#include "CodeGen.h"
#include "AST.h"
#include <cassert>

////////////////////////////////////////
#pragma mark - IR Visitor
////////////////////////////////////////

void ToIRVisitor::visit (Factor& Node) {
    if (Node.getKind () == Factor::Ident) {
        V = nameMap[Node.getVal ()];
    } else {
        int intVal;
        Node.getVal ().getAsInteger (10, intVal);
        V = llvm::ConstantInt::get (Int32Type, intVal, true);
    }
}

void ToIRVisitor::visit (BinaryOp& Node) {
    // get left and right operands
    Node.getLeft ()->accept (*this);
    llvm::Value* Left = V;
    Node.getRight ()->accept (*this);
    llvm::Value* Right = V;

    switch (Node.getOperator ()) {
    case BinaryOp::Plus: V = Builder.CreateNSWAdd (Left, Right); break;
    case BinaryOp::Minus: V = Builder.CreateNSWSub (Left, Right); break;
    case BinaryOp::Mul: V = Builder.CreateNSWMul (Left, Right); break;
    case BinaryOp::Div: V = Builder.CreateSDiv (Left, Right); break;
    }
}

void ToIRVisitor::visit (With& Node) {
    llvm::FunctionType* ReadFnType = llvm::FunctionType::get (Int32Type, { PtrType }, false);
    llvm::Function* ReadFn =
    llvm::Function::Create (ReadFnType, llvm::GlobalValue::ExternalLinkage, "calc_read", M);

    for (auto it = Node.begin (); it != Node.end (); it++) {
        llvm::StringRef Curr = *it; // remember an array of SmallVec's with 8 len

        llvm::Constant* StrText   = llvm::ConstantDataArray::getString (M->getContext (), Curr);
        llvm::GlobalVariable* Str = new llvm::GlobalVariable (*M, StrText->getType (), true,
        llvm::GlobalValue::PrivateLinkage, StrText, llvm::Twine (Curr).concat (".str"));


        llvm::CallInst* Call = Builder.CreateCall (ReadFnType, ReadFn, Str);
        nameMap[Curr]        = Call;
    }

    Node.getExpr ()->accept (*this);
}

void ToIRVisitor::visit (DefDecl& Node) {
    llvm::StringRef FnName                             = Node.getFnName ();
    llvm::SmallVector<llvm::StringRef, 8> FunctionVars = Node.getVars ();
    // Add the function name and arguments to the list of JIT'd functions.
    JITFns[FnName] = FunctionVars.size ();

    // Create the actual user defined function.
    Function* DefFunc = genUserDefinedFunction (FnName);
    assert (DefFunc);

    // Set up a new basic block to insert the user defined function into.
    auto* BB = llvm::BasicBlock::Create (M->getContext (), "entry", DefFunc);
    Builder.SetInsertPoint (BB);

    // Set the names for all function arguments.
    unsigned FIdx = 0;
    for (auto& FArg : DefFunc->args ()) {
        nameMap[FunctionVars[FIdx]] = &FArg;
        FArg.setName (FunctionVars[FIdx++]);
    }

    // Generate binary operations on arguments.
    Node.getExpr ()->accept (*this);
}


void ToIRVisitor::visit (FuncCallFromDef& Node) {
    // Creation of calc_expr_func
    llvm::StringRef CalcExprFunName = "calc_expr_func";

    // Prepare the function signature for the temporary function that will
    // call the user defined function.
    auto* CalcExprFunTy = llvm::FunctionType::get (Int32Type, {}, false);
    Function* CalcExprFun =
    Function::Create (CalcExprFunTy, llvm::GlobalValue::ExternalLinkage, CalcExprFunName, M);

    auto* BB = llvm::BasicBlock::Create (M->getContext (), "entry", CalcExprFun);
    Builder.SetInsertPoint (BB);

    // Get the original function definition. (we have a llvm::Module::getFunction(), which lets us know if function is already defined in that mod)
    llvm::StringRef CalleeFnName = Node.getFnName ();
    Function* CalleeFn           = genUserDefinedFunction (CalleeFnName);
    assert (CalleeFn);

    // Set the parameters for the function call.
    auto CalleeFnVars = Node.getArgs ();
    llvm::SmallVector<llvm::Value*> IntParams;
    for (unsigned i = 0, end = CalleeFnVars.size (); i != end; ++i) {
        int ArgsToIntType;
        CalleeFnVars[i].getAsInteger (10, ArgsToIntType);
        llvm::Value* IntParam = llvm::ConstantInt::get (Int32Type, ArgsToIntType, true);
        IntParams.push_back (IntParam);
    }

    // Now lets generate a call to the Function we got as well as the params with the custom name
    // "calc_expr_res" Also Ret the llvm::Value we got from the Call Inst and store it inside calc_expr_res
    llvm::Value* Res = Builder.CreateCall (CalleeFn, IntParams, "calc_expr_res");
    Builder.CreateRet (Res);
}


void ToIRVisitor::run (AST* Tree) {
    Tree->accept (*this);
    Builder.CreateRet (V);
}


////////////////////////////////////////
#pragma mark - CodeGen / JIT
////////////////////////////////////////

bool CodeGen::compileToIR (AST* Tree, llvm::Module* M, StringMap<size_t>& JITtedFunctions) {
    // Our init+Run
    ToIRVisitor Visitor (M, JITtedFunctions);
    Visitor.run (Tree);
    M->print (llvm::outs (), nullptr);

    // later make this a check to check if any of the LLVM IR Builders encountered errors
    return true;
}

void CodeGen::prepareCalculationCallFunc (AST* FuncCall, llvm::Module* M, llvm::StringRef Name, StringMap<size_t>& JITFns) {
    ToIRVisitor ToIR (M, JITFns);
    ToIR.genFuncEvaluationCall (FuncCall);
    M->print (llvm::outs (), nullptr);
}


Function* ToIRVisitor::genUserDefinedFunction (llvm::StringRef FnName) {
    // If a function from the module with the user defined name exists,
    // let's use that function.
    if (Function* FuncFromModule = M->getFunction (FnName))
        return FuncFromModule;

    // Otherwise, let's create the user defined function based off the
    // function name and stored arguments.
    Function* UserDefinedFunction = nullptr;
    auto FnNameToArgCount         = JITFns.find (FnName);

    // If Function is found create a new function of that name, and of type Int32Type[Count-We-Found-From-Map]
    if (FnNameToArgCount != JITFns.end ()) {
        std::vector<llvm::Type*> IntArgs (FnNameToArgCount->second, Int32Type);
        auto* FuncType = llvm::FunctionType::get (Int32Type, IntArgs, false);
        UserDefinedFunction = Function::Create (FuncType, llvm::GlobalValue::ExternalLinkage, FnName, M);
    }

    return UserDefinedFunction;
}