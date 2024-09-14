add_amanlang_tool(amanlang
    Driver.cc
)

target_link_libraries(amanlang
  PRIVATE
  amanlangBasic
  amanlangLexer
  amanlangParser
  amanlangSema
  amanlangCodeGen
)