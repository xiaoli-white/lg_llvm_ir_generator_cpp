#include <iostream>

#include <lg-cpp-binding.h>

#include "llvm_ir_gen.h"

int main()
{
    std::string code = "function i32 main(){}{"
                       "entry:"
                       "\t%1 = add i32 1, i32 2"
                       "\t%2 = cmp le, i32 1, i32 2"
                       "\tconditional_jump if_false, i1 %2, label exit "
                       // "\tgoto label exit "
                       "exit:"
                       "\treturn i32 %1"
                       "}";
    auto module = lg::ir::parser::parse(code);
    lg::ir::IRDumper dumper;
    dumper.visitModule(module, std::string(""));

    llvm::LLVMContext context;
    auto llvmModule = new llvm::Module("", context);
    lg::llvm_ir_gen::LLVMIRGenerator generator(module, &context, llvmModule);
    generator.generate();
    llvmModule->print(llvm::outs(), nullptr);
    lg::llvm_ir_gen::compile(llvmModule, "x86_64-pc-linux-gnu", "a.out");
    return 0;
}
