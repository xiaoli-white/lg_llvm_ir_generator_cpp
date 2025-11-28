//
// Created by xiaoli on 2025/11/25.
//

#ifndef LG_LLVM_IR_GENERATOR_CPP_LLVM_IR_GEN_H
#define LG_LLVM_IR_GENERATOR_CPP_LLVM_IR_GEN_H
#include <lg-cpp-binding.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/WithColor.h>

#include "clang/Driver/Driver.h"
#include "clang/Driver/Compilation.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include "llvm/IR/GlobalVariable.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/Support/raw_ostream.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <llvm/Support/VirtualFileSystem.h>

namespace lg::llvm_ir_gen
{
    class LLVMIRGenerator final: public ir::IRVisitor
    {
    private:
        ir::IRModule* module;
        llvm::LLVMContext* context;
        llvm::Module* llvmModule;
        llvm::IRBuilder<>* builder;
        llvm::Function* currentFunction = nullptr;
        std::stack<std::any> stack;
        std::unordered_map<ir::base::IRGlobalVariable*, llvm::GlobalVariable*> irGlobalVariable2LLVMGlobalVariable;
        std::unordered_map<ir::base::IRBasicBlock*, llvm::BasicBlock*> irBlock2LLVMBlock;
        std::unordered_map<ir::value::IRRegister*, llvm::Value*> register2Value;
    public:
        LLVMIRGenerator(ir::IRModule* module,llvm::LLVMContext* context, llvm::Module* llvmModule);
        ~LLVMIRGenerator() override;
        std::string generate();

        std::any visitModule(ir::IRModule* module, std::any additional) override;
        std::any visitGlobalVariable(ir::base::IRGlobalVariable* irGlobalVariable, std::any additional) override;
        std::any visitFunction(ir::function::IRFunction* irFunction, std::any additional) override;
        std::any visitBinaryOperates(ir::instruction::IRBinaryOperates* irBinaryOperates, std::any additional) override;
        std::any visitGetElementPointer(ir::instruction::IRGetElementPointer* irGetElementPointer, std::any additional) override;
        std::any visitCompare(ir::instruction::IRCompare* irCompare, std::any additional) override;
        std::any visitConditionalJump(ir::instruction::IRConditionalJump* irConditionalJump, std::any additional) override;
        std::any visitGoto(ir::instruction::IRGoto* irGoto, std::any additional) override;
        std::any visitInvoke(ir::instruction::IRInvoke* irInvoke, std::any additional) override;
        std::any visitReturn(ir::instruction::IRReturn* irReturn, std::any additional) override;
        std::any visitRegister(ir::value::IRRegister* irRegister, std::any additional) override;
        std::any visitIntegerConstant(ir::value::constant::IRIntegerConstant* irIntegerConstant, std::any additional) override;
        std::any visitIntegerType(ir::type::IRIntegerType* irIntegerType, std::any additional) override;
    };
    void compile(llvm::Module* module, std::string triple, std::string output);
}

#endif //LG_LLVM_IR_GENERATOR_CPP_LLVM_IR_GEN_H