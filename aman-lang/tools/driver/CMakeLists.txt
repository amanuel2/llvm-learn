add_amanlang_tool(amanlang
    Driver.cc
    SUPPORT_PLUGINS
)

# We must link all transforms since we are looping through them
set(LLVM_LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD}
  AggressiveInstCombine Analysis AsmParser
  BitWriter CodeGen Core Coroutines IPO IRReader
  InstCombine Instrumentation MC ObjCARCOpts Remarks
  ScalarOpts Support Target TransformUtils Vectorize
  Passes)

target_link_libraries(amanlang
  PRIVATE
  amanlangBasic
  amanlangLexer
  amanlangParser
  amanlangSema
  amanlangCodeGen
)