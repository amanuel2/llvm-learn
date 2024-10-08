macro(add_amanlang_subdirectory name)
    add_llvm_subdirectory(AMANLANG TOOL ${name})
endmacro()

macro(add_amanlang_executable name)
    add_llvm_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE
        clangTooling
        clangBasic
        clangFrontend
        clangSerialization
        clangASTMatchers
        clangAST
        clangLex
        LLVMSupport
    )
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "-fno-rtti")
endmacro()

macro(add_amanlang_tool name)
    add_amanlang_executable(${name} ${ARGN})
    install(TARGETS ${name}
    RUNTIME DESTINATION bin
    COMPONENT ${name})
endmacro()


macro(add_amanlang_library name)
    if (BUILD_SHARED_LIBS)
        set(LIBTYPE SHARED)
    else()
        set(LIBTYPE STATIC)
    endif()

    llvm_add_library(${name} ${LIBTYPE} ${ARGN})
    if (TARGET ${name}) # check target exists
        target_link_libraries(${name} INTERFACE ${LLVM_COMMON_LIBS})
        install(TARGETS ${name}
        COMPONENT ${name}
        LIBRARY DESTINATION lib${LLVM_LIBDIR_SUFFIX}
        ARCHIVE DESTINATION lib${LLVM_LIBDIR_SUFFIX}
        RUNTIME DESTINATION bin)
    else()
        add_custom_target(${name})
    endif()
endmacro()

