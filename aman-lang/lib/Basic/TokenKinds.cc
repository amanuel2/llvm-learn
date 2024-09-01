#include "amanlang/Basic/TokenKinds.h"
#include "llvm/Support/ErrorHandling.h"

using namespace amanlang::tok;

static const char * const TokNames[] = {
    #define TOK(ID) #ID,
    #define KEYWORD(ID, FLAG) #ID,
    #include "amanlang/Basic/TokenKinds.def"
    #undef TOK
    #undef KEYWORD
    nullptr
};

const char* getTokenName(TokenKind Kind) {
    if (Kind < NUM_TOKENS) return TokNames[Kind];
    llvm_unreachable("TokenKind is not present in enum");
    return nullptr;
}

const char* getPunctuatorSpelling (TokenKind Kind) {
    switch(Kind) {
        #define PUNCTUATOR(ID, SP) case ID: return SP;
        #include "amanlang/Basic/TokenKinds.def"
        #undef PUNCTUATOR
        default: break;
    }
    return nullptr;
}

const char* getKeywordSpelling (TokenKind Kind) {
    switch (Kind) {
        #define KEYWORD(ID, FLAG)  case kw_ ## ID: return #ID;
        #include "amanlang/Basic/TokenKinds.def"
        #undef KEYWORD
        default: break;
    }
    return nullptr;
}
