#include <iostream>

#include <lg-cpp-binding.h>

#include "llvm_ir_gen.h"

int main()
{
    std::string code = "global aaa = i32 1 "
                       "const global bbb = i32 2"
                       "function i32 main(){}{"
                       "entry:"
                       "\t%1 = add i32 1, i32 2"
                       "\t%2 = cmp le, i32 1, i32 2"
                       "\tconditional_jump if_false, i1 %2, label exit "
                       // "\tgoto label exit "
                       "exit:"
                       "\treturn i32 %1"
                       "}";
    const auto module = lg::ir::parser::parse(code);
    std::cout << "==============LG IR============" << std::endl;
    lg::ir::IRDumper dumper;
    dumper.visitModule(module, std::string(""));

    llvm::LLVMContext context;
    const auto llvmModule = new llvm::Module("", context);
    lg::llvm_ir_gen::LLVMIRGenerator generator(module, &context, llvmModule);
    generator.generate();
    std::cout << "===========LLVM IR=============" << std::endl;
    llvmModule->print(llvm::outs(), nullptr);
    lg::llvm_ir_gen::compile(llvmModule, "x86_64-pc-linux-gnu", "a.out");
    return 0;
}
