#include "amanlang/CodeGen/CGTbaa.h"
#include "amanlang/AST/AST.h"
#include "amanlang/CodeGen/CGModule.h"

namespace amanlang {

CGTbaa::CGTbaa (CGModule& CGM) : CGM (CGM), Builder (CGM.getLLVMCtx ()) {
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGTbaa (Complex getters)
/////////////////////////////////////////////////////////////////////////////

llvm::MDNode* CGTbaa::getRoot () {
    // lazy loading
    if (!Root)
        Root = Builder.createTBAARoot ("Aman TBAA");
    return Root;
}

llvm::MDNode* CGTbaa::getTypeInfo (TypeDecl* Ty) {
    // First check if in cache
    if (llvm::MDNode* N = MetadataCache[Ty])
        return N;

    // First check if Pervasive (Built-In) Scalar Type
    if (auto* Pervasive = llvm::dyn_cast<PervasiveTypeDecl> (Ty)) {
        StringRef Name = Pervasive->getName ();
        return createScalarTypeNode (Pervasive, Name, getRoot ());
    }

    // Check if its a Pointer (boils down to a (char, 0) aka scalar node)
    if (auto* Pointer = llvm::dyn_cast<PointerTypeDecl> (Ty)) {
        StringRef Name = "any pointer";
        return createScalarTypeNode (Pointer, Name, getRoot ());
    }

    // Check if its an array (Boils down to a bunch of members with simillar type and offsets)
    if (auto* Array = llvm::dyn_cast<ArrayTypeDecl> (Ty)) {
        StringRef Name = Array->getType ()->getName ();
        return createScalarTypeNode (Array, Name, getRoot ());
    }

    // Here comes the hard part
    // For records there might be different types which = diff offsets
    if (auto* Record = llvm::dyn_cast<RecordTypeDecl> (Ty)) {
        llvm::SmallVector<std::pair<llvm::MDNode*, uint64_t>, 4> Fields;
        auto* Rec = llvm::cast<llvm::StructType> (CGM.convertType (Record));
        const llvm::StructLayout* Layout =
        CGM.getModule ()->getDataLayout ().getStructLayout (Rec);

        unsigned Idx = 0;
        for (const auto& F : Record->getFields ()) {
            uint64_t Offset = Layout->getElementOffset (Idx);
            Fields.emplace_back (getTypeInfo (F.getType ()), Offset);
            ++Idx;
        }
        StringRef Name = CGM.mangleName (Record);
        return createStructTypeNode (Record, Name, Fields);
    }


    return nullptr;
}

llvm::MDNode* CGTbaa::getAccessTagInfo (TypeDecl* Ty) {
    return getTypeInfo (Ty);
}


/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGTbaa (Create Scalar vs Struct MD Nodes)
/////////////////////////////////////////////////////////////////////////////

llvm::MDNode* CGTbaa::createScalarTypeNode (TypeDecl* Ty, StringRef Name, llvm::MDNode* Parent) {
    llvm::MDNode* MD         = Builder.createTBAAScalarTypeNode (Name, Parent);
    return MetadataCache[Ty] = MD;
}

llvm::MDNode* CGTbaa::createStructTypeNode (TypeDecl* Ty,
StringRef Name,
ArrayRef<std::pair<llvm::MDNode*, uint64_t>> Fields) {
    llvm::MDNode* MD         = Builder.createTBAAStructTypeNode (Name, Fields);
    return MetadataCache[Ty] = MD;
}

} // namespace amanlang