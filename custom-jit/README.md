# Building JIT Compiler class


The ORC API uses layers that are stacked together. 

1) The lowest level is the object-linking layer, represented by the `llvm::orc::RTDyldObjectLinkingLayer` class. It is responsible for linking in-memory objects and turning them into executable code. 

The memory required for this task is managed by an instance of the `MemoryManager` interface.

Above the object-linking layer is the compile layer, which is responsible for creating an in-memory object file. 

2) The `llvm::orc::IRCompileLayer` class takes an IR module as input and compiles it to an object file.

<i>Both of these layers already form the core of a JIT compiler: they add an LLVM IR module as input, which is compiled and linked in memory. To add extra functionality, we can incorporate more layers on top of both layers.</i>

For example, the `CompileOnDemandLayer` class splits a module so that only the requested functions are compiled