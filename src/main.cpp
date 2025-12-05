#include <iostream>

#include <lg/parser.h>
#include <lg/dumper.h>

#include "llvm_ir_gen.h"

int main()
{
    std::string code = "global aaa = i32 1 "
                       "const global bbb = i32 2"
                       "global structTest = constant structure A { i32 1, constant structure B { u64 2 } }"
                       "function i32 main(){}{"
                       "entry:"
                       "\t%114 = stack_alloc i32"
                       "\t%115 = ptrtoptr i32* %114 to u8*"
                       "\t%116 = load u8* %115"
                       "\t%1 = add i32 1, i32 2"
                       "\t%2 = cmp le, i32 1, i32 2"
                       "\tnop"
                       "\tconditional_jump if_false, i1 %2, label exit "
                       // "\tgoto label exit "
                       "exit:"
                       "\t%3 = invoke void* funcref malloc(i32 4)"
                       "\tinvoke void funcref free(void* %3)"
                       "\t%fmtstr = getelementptr globalref f, i32 0, i32 0"
                       "\t%ret = invoke i32 funcref printf(u8* %fmtstr, i32 1)"
                       "\treturn i32 %1"
                       "}"
                       "extern function void* malloc(i32 size)"
                       "extern function void free(void* ptr)"
                       "structure A {"
                       "\ti32 a,"
                       "\tstructure B b"
                       "}"
                       "structure B {"
                       "\tu64 c"
                       "}"
                       "extern function i32 printf(u8* fmt, ...)"
                       "global f = string \"%d\n\"";
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
