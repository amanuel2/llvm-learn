add_llvm_component_library(LLVMPProfiler
  PProfiler.cc

  LINK_COMPONENTS
  Core
  Support
)