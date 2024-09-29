#include "TableGenBackends.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"
#include <algorithm>

namespace {
class GlobalEmitter {
public:
  explicit GlobalEmitter(llvm::RecordKeeper &Records) : Records(Records) {};
  void run(llvm::raw_ostream &os);

private:
  llvm::RecordKeeper &Records;
  void emitFlagsFragment(llvm::raw_ostream &os);
  void emitTokenKind(llvm::raw_ostream &os);
  void emitKeywordFilter(llvm::raw_ostream &os);
};
} // namespace

namespace {

void GlobalEmitter::run(llvm::raw_ostream &os) {
  Records.startTimer("Emiting flags");
  this->emitFlagsFragment(os);

  Records.startTimer("Emitting Tokens"); // last timer automatically stops
  this->emitTokenKind(os);

  Records.startTimer("Emitting Keywords");
  this->emitKeywordFilter(os);

  Records.stopTimer();
}

void GlobalEmitter::emitFlagsFragment(llvm::raw_ostream &os) {
  os << "#ifdef GET_FLAGS\n";
  os << "#undef GET_FLAGS\n";
  for (llvm::Record *CC : Records.getAllDerivedDefinitions("Flag")) {
    auto Name = CC->getValueAsString("Name");
    int64_t Val = CC->getValueAsInt("Val");
    os << Name << " = " << llvm::format_hex(Val, 2) << ",\n";
  }
  os << "#endif\n";
};

// Emission of TokenKinds.def file
// in Aman-Lang
void GlobalEmitter::emitTokenKind(llvm::raw_ostream &os) {
  os << "#ifdef GET_TOK_KIND_DECL\n"
     << "#define GET_TOK_KIND_DECL\n"

     /*
      enum TokenKind : unsigned short {
       kw_while,
       kw_for,
       ...

       NUM_TOKENS
      }
     */
     << "namespace tok {\n"
     << "  enum TokenKind : unsigned short {\n";

  for (llvm::Record *Rec : Records.getAllDerivedDefinitions("Token")) {
    auto Name = Rec->getValueAsString("Name");
    if (Rec->isSubClassOf("Keyword"))
      os << "    kw_";
    os << Name << ",\n";
  }
  os << "    NUM_TOKENS\n"
     << "  };\n";

  // Function declarations
  os << "  const char *getTokenName(TokenKind Kind);\n"
     << "  const char *getPunctuatorSpelling(TokenKind Kind);\n"
     << "  const char *getKeywordSpelling(TokenKind Kind);\n"

     << "}\n"
     << "#endif /* GET_TOK_KIND_DECL */\n";

  //
  // TOKEN KIND DEFINTIONS
  os << "#ifdef GET_TOK_KIND_DEF\n"
     << "#define GET_TOK_KIND_DEF\n";

  /*
   Database file equivilant (Previous Impl)
   ------------------------
   static const char* const TokNames[] = {
    #define TOK(ID) #ID,
    #define KEYWORD(ID, FLAG) #ID,
    #include "amanlang/Basic/TokenKinds.def"
    #undef TOK
    #undef KEYWORD

    nullptr
  };
   */
  os << "static const char * const TokNames[] = {\n";
  for (llvm::Record *Record : Records.getAllDerivedDefinitions("Token")) {
    os << "  \"" << Record->getValueAsString("Name") << "\",\n";
  }
  os << "};\n\n";

  // Get Token Names
  os << "const char *tok::getTokenName(TokenKind Kind) "
        "{\n"
     << "  if (Kind <= tok::NUM_TOKENS)\n"
     << "    return TokNames[Kind];\n"
     << "  llvm_unreachable(\"unknown TokenKind\");\n"
     << "  return nullptr;\n"
     << "};\n\n";

  /*
   Database file equivilant (Previous Impl)
   ------------------------
    const char* getPunctuatorSpelling (TokenKind Kind) {
        switch (Kind) {

            #define PUNCTUATOR(ID, SP) case ID: return SP;
            #include "amanlang/Basic/TokenKinds.def"
            #undef PUNCTUATOR

            default: break;
        }
        return nullptr;
    }
   */

  os << "const char *tok::getPunctuatorSpelling(TokenKind Kind) {\n"
     << "  switch (Kind) {\n";
  for (auto *Record : Records.getAllDerivedDefinitions("Punctuator")) {
    os << "    " << Record->getValueAsString("Name") << ": return \""
       << Record->getValueAsString("Spelling") << "\";\n";
  }
  os << "    default: break;\n"
     << "  }\n"
     << "  return nullptr;\n"
     << "};\n\n";

  /*
   Database file equivilant (Previous Impl)
   ------------------------
    const char* getKeywordSpelling (TokenKind Kind) {
        switch (Kind) {

            #define KEYWORD(ID, FLAG) case kw_##ID: return #ID;
            #include "amanlang/Basic/TokenKinds.def"
            #undef PUNCTUATOR

            default: break;
        }
        return nullptr;
    }
   */

  os << "const char *tok::getKeywordSpelling(TokenKind "
        "Kind) {\n"
     << "  switch (Kind) {\n";
  for (llvm::Record *Record : Records.getAllDerivedDefinitions("Keyword")) {
    os << "    kw_" << Record->getValueAsString("Name") << ": return \""
       << Record->getValueAsString("Name") << "\";\n";
  }
  os << "    default: break;\n"
     << "  }\n"
     << "  return nullptr;\n"

     << "};\n\n";
  os << "#endif\n";
}

void GlobalEmitter::emitKeywordFilter(llvm::raw_ostream &os) {
  // LETS GRAB ALL THE KEYWORDS FIRST
  /*

  JSON:
    AllTokenFilter : {
        "Tokens": [
            "Name": "", Val: 1, "Flags": [],
            "Name": "", Val: 2, "Flags": [],
            ...
        }
    }
  TBLGEN:
    class TokenFilter<list<Token> tokens> {
        string FunctionName;
        list<Token> Tokens = tokens;
    }

    Info: llvm::ListInit => [AL, AH, CL] - Represent a list of defs of insts
  */
  std::vector<llvm::Record *> AllTokenFilter =
      Records.getAllDerivedDefinitionsIfDefined("TokenFilter"); /* If-Defined */
  if (AllTokenFilter.empty())
    return;

  auto *TokenFilter = llvm::dyn_cast_or_null<llvm::ListInit>(
      AllTokenFilter[0]->getValue("Tokens")->getValue()); //
  if (!TokenFilter)
    return;

  // Collect the keyword/flag values.
  std::vector<std::pair<llvm::StringRef, uint64_t>> Table;

  for (size_t I = 0, E = TokenFilter->size(); I < E; ++I) {
    llvm::Record *Record = TokenFilter->getElementAsRecord(I);
    auto Name = Record->getValueAsString("Name");
    uint64_t Val = 0;
    llvm::ListInit *Flags = nullptr;

    /// About llvm::RecordVal => represents a field in a record, including its
    /// name, type, value, and source location.
    if (llvm::RecordVal *F = Record->getValue("Flags"))
      Flags = llvm::dyn_cast_or_null<llvm::ListInit>(F->getValue());

    if (Flags) {
      for (size_t I = 0, E = Flags->size(); I < E; ++I) {
        Val |= Flags->getElementAsRecord(I)->getValueAsInt("Val");
      }
    }
    Table.emplace_back(Name, Val);
  }

  using CompKeys = std::pair<llvm::StringRef, uint64_t>;
  llvm::sort(
      Table.begin(), Table.end(),
      [](const CompKeys A, const CompKeys B) { return A.first < B.first; });

  // NOW LETS OUTPUT KEYWORD FILTERS
  os << "#ifdef GET_KEYWORD_FILTER\n"
     << "#undef GET_KEYWORD_FILTER\n"

     << "bool lookupKeyword(llvm::StringRef Keyword, unsigned &Value) {\n"
     << "  struct Entry {\n"
     << "    unsigned Value;\n"
     << "    llvm::StringRef Keyword;\n"
     << "  };\n"

     << "  static const Entry Table[" << Table.size() << "] = {\n";
  for (const auto &[Keyword, Value] : Table) {
    os << "    { " << Value << ", llvm::StringRef(\"" << Keyword << "\", "
       << Keyword.size() << ") },\n";
  }
  os << "  };\n\n";

  // Find lower bound of the Keyword in the sorted array (we did via llvm::sort)
  os << "  const Entry *E = std::lower_bound(&Table[0], &Table[" << Table.size()
     << "], Keyword, [](const Entry &A, const "
        "StringRef "
        "&B) {\n";
  os << "    return A.Keyword < B;\n";
  os << "  });\n";

  // same as if (E != Table.end()), aka we found the keyword in the list
  os << "  if (E != &Table[" << Table.size() << "]) {\n";
  os << "    Value = E->Value;\n";
  os << "    return true;\n";
  os << "  }\n";
  os << "  return false;\n";
  os << "}\n";
}
} // namespace

// Record Keeper is representation of records in C++ for TBLGen
// has methods like  addClass, addDef, etc..
void EmitTokensAndKeywordFilter(llvm::RecordKeeper &RK, llvm::raw_ostream &OS) {
  emitSourceFileHeader("Token Kind and Keyword Filter "
                       "Implementation Fragment",
                       OS);
  GlobalEmitter(RK).run(OS);
}