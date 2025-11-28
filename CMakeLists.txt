cmake_minimum_required(VERSION 4.0)
project(lg_llvm_ir_generator_cpp)

set(CMAKE_CXX_STANDARD 20)

find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
find_package(antlr4-runtime REQUIRED)

set(llvm_components core irreader support analysis passes codegen target mc object linker option)
llvm_map_components_to_libnames(llvm_libs ${llvm_components})

include_directories(${ANTLR4_INCLUDE_DIR} lg_cpp_binding/include/ include/)

add_subdirectory(lg_cpp_binding)

add_executable(lg_llvm_ir_generator_cpp
        src/main.cpp
        include/llvm_ir_gen.h
        src/llvm_ir_gen.cpp
)

target_link_libraries(lg_llvm_ir_generator_cpp PRIVATE
        clang-cpp
        LLVM
)
target_link_libraries(lg_llvm_ir_generator_cpp PRIVATE lg)

if (WIN32)
    target_compile_definitions(llvm_ir_generator PRIVATE _WINDLL _MBCS)
    set_target_properties(llvm_ir_generator PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    target_link_libraries(llvm_ir_generator PRIVATE
            LLVMWindowsDriver
            version
    )
endif ()