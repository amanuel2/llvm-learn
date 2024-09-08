#pragma once

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "amanlang/AST/AST.h"

namespace amanlang {
    class Decl;

    class Scope {
        public:
        Scope (Scope* Parent = nullptr) : Parent (Parent) {}
        ~Scope () {}

        Decl* lookup (llvm::StringRef Name) const {
            auto It = Symbols.find (Name);
            if (It != Symbols.end ())
                return It->second;
            if (Parent)
                return Parent->lookup (Name);
            return nullptr;
        }

        bool insert (Decl* D) {
            return Symbols.insert({ D->getName(), D }).second;
        }

        Scope* getParent () const {
            return Parent;
        }

        private:
        llvm::StringMap<Decl*> Symbols;
        Scope* Parent;
    };
}