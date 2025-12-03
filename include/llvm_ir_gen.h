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
        std::any visitAssembly(ir::instruction::IRAssembly* irAssembly, std::any additional) override;
        std::any visitBinaryOperates(ir::instruction::IRBinaryOperates* irBinaryOperates, std::any additional) override;
        std::any visitUnaryOperates(ir::instruction::IRUnaryOperates* irUnaryOperates, std::any additional) override;
        std::any visitGetElementPointer(ir::instruction::IRGetElementPointer* irGetElementPointer, std::any additional) override;
        std::any visitCompare(ir::instruction::IRCompare* irCompare, std::any additional) override;
        std::any visitConditionalJump(ir::instruction::IRConditionalJump* irConditionalJump, std::any additional) override;
        std::any visitGoto(ir::instruction::IRGoto* irGoto, std::any additional) override;
        std::any visitInvoke(ir::instruction::IRInvoke* irInvoke, std::any additional) override;
        std::any visitReturn(ir::instruction::IRReturn* irReturn, std::any additional) override;
        std::any visitLoad(ir::instruction::IRLoad* irLoad, std::any additional) override;
        std::any visitStore(ir::instruction::IRStore* irStore, std::any additional) override;
        std::any visitNop(ir::instruction::IRNop* irNop, std::any additional) override;
        std::any visitSetRegister(ir::instruction::IRSetRegister* irSetRegister, std::any additional) override;
        std::any visitStackAllocate(ir::instruction::IRStackAllocate* irStackAllocate, std::any additional) override;
        std::any visitTypeCast(ir::instruction::IRTypeCast* irTypeCast, std::any additional) override;
        std::any visitPhi(ir::instruction::IRPhi* irPhi, std::any additional) override;
        std::any visitSwitch(ir::instruction::IRSwitch* irSwitch, std::any additional) override;
        std::any visitRegister(ir::value::IRRegister* irRegister, std::any additional) override;
        std::any visitIntegerConstant(ir::value::constant::IRIntegerConstant* irIntegerConstant, std::any additional) override;
        std::any visitFloatConstant(ir::value::constant::IRFloatConstant* irFloatConstant, std::any additional) override;
        std::any visitDoubleConstant(ir::value::constant::IRDoubleConstant* irDoubleConstant, std::any additional) override;
        std::any visitNullptrConstant(ir::value::constant::IRNullptrConstant* irNullptrConstant, std::any additional) override;
        std::any visitStringConstant(ir::value::constant::IRStringConstant* irStringConstant, std::any additional) override;
        std::any visitArrayConstant(ir::value::constant::IRArrayConstant* irArrayConstant, std::any additional) override;
        std::any visitIntegerType(ir::type::IRIntegerType* irIntegerType, std::any additional) override;
        std::any visitFloatType(ir::type::IRFloatType* irFloatType, std::any additional) override;
        std::any visitDoubleType(ir::type::IRDoubleType* irDoubleType, std::any additional) override;
        std::any visitVoidType(ir::type::IRVoidType* irVoidType, std::any additional) override;
        std::any visitArrayType(ir::type::IRArrayType* irArrayType, std::any additional) override;
        std::any visitPointerType(ir::type::IRPointerType* irPointerType, std::any additional) override;
        // std::any visitStructureType(ir::type::IRStructureType* irStructureType, std::any additional) override;
        // std::any visitFunctionReferenceType(ir::type::IRFunctionReferenceType* irFunctionReferenceType, std::any additional) override;
    };
    void compile(llvm::Module* module, std::string triple, std::string output);
}

#endif //LG_LLVM_IR_GENERATOR_CPP_LLVM_IR_GEN_H