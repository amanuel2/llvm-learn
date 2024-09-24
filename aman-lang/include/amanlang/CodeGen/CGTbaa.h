#pragma once
#include "amanlang/AST/AST.h"

#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Metadata.h"


// ADT's
using llvm::ArrayRef;
using llvm::DenseMap;
using llvm::StringRef;

namespace amanlang {

class CGModule;
class CGTbaa {
    public:
    explicit CGTbaa (CGModule& CGM);

    // complex-getters
    llvm::MDNode* getRoot ();
    llvm::MDNode* getTypeInfo (TypeDecl* Ty);
    llvm::MDNode* getAccessTagInfo (TypeDecl* Ty);

    private:
    CGModule& CGM;

    // Metadata node Builder (synonomous with the IR builder)
    llvm::MDBuilder Builder;

    // The root node of the TBAA hierarchy
    llvm::MDNode* Root;


    // Creating Scalar (offset 0) vs Struct-Type functionality
    // pair of {MDNode(val), offset} []
    llvm::MDNode* createScalarTypeNode (TypeDecl* Ty, StringRef Name, llvm::MDNode* Parent);
    llvm::MDNode* createStructTypeNode (TypeDecl* Ty,
    StringRef Name,
    ArrayRef<std::pair<llvm::MDNode*, uint64_t>> Fields);

    // Caching
    llvm::DenseMap<TypeDecl*, llvm::MDNode*> MetadataCache;
};
} // namespace amanlang