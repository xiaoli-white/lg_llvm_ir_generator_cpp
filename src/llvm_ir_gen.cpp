//
// Created by xiaoli on 2025/11/25.
//

#include <llvm_ir_gen.h>
#include <ranges>

namespace lg::llvm_ir_gen
{
    LLVMIRGenerator::LLVMIRGenerator(ir::IRModule* module, llvm::LLVMContext* context, llvm::Module* llvmModule) :
        module(module), context(context), llvmModule(llvmModule)
    {
        builder = new llvm::IRBuilder(*context);
    }

    LLVMIRGenerator::~LLVMIRGenerator()
    {
        delete builder;
    }


    std::string LLVMIRGenerator::generate()
    {
        visit(module, nullptr);
        return "";
    }

    std::any LLVMIRGenerator::visitModule(ir::IRModule* module, std::any additional)
    {
        for (auto& func : module->functions | std::views::values)
        {
            visit(func->returnType, additional);
            auto returnType = std::any_cast<llvm::Type*>(stack.top());
            stack.pop();
            std::vector<llvm::Type*> args;
            for (auto& arg : func->args)
            {
                visit(arg, additional);
                args.push_back(std::any_cast<llvm::Type*>(stack.top()));
                stack.pop();
            }
            auto functionType = llvm::FunctionType::get(returnType, args, false);
            llvm::Function* llvmFunction = llvm::Function::Create(
                functionType,
                llvm::Function::ExternalLinkage,
                func->name,
                llvmModule
            );
            for (auto& arg : llvmFunction->args())arg.setName(func->args[arg.getArgNo()]->name);
        }
        for (auto& func : module->functions | std::views::values)
        {
            visit(func, additional);
        }
        return nullptr;
    }

    std::any LLVMIRGenerator::visitFunction(ir::function::IRFunction* irFunction, std::any additional)
    {
        currentFunction = llvmModule->getFunction(irFunction->name);
        for (auto& block : irFunction->cfg->basicBlocks | std::views::values)
        {
            llvm::BasicBlock* llvmBlock = llvm::BasicBlock::Create(*context, block->name, currentFunction);
            builder->SetInsertPoint(llvmBlock);
            for (auto& instruction : block->instructions)visit(instruction, additional);
        }
        register2Value.clear();
        return nullptr;
    }

    std::any LLVMIRGenerator::visitBinaryOperates(ir::instruction::IRBinaryOperates* irBinaryOperates,
                                                  std::any additional)
    {
        visit(irBinaryOperates->operand1, additional);
        auto* operand1 = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        visit(irBinaryOperates->operand2, additional);
        auto* operand2 = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        llvm::Value* result;
        switch (irBinaryOperates->op)
        {
        case ir::instruction::IRBinaryOperates::Operator::ADD:
            {
                result = builder->CreateAdd(operand1, operand2);
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::SUB:
            {
                result = builder->CreateSub(operand1, operand2);
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::MUL:
            {
                result = builder->CreateMul(operand1, operand2);
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::DIV:
            {
                auto* type = irBinaryOperates->operand1->getType();
                if (const auto* integerType = dynamic_cast<ir::type::IRIntegerType*>(type))
                {
                    if (integerType->_unsigned) result = builder->CreateUDiv(operand1, operand2);
                    else result = builder->CreateSDiv(operand1, operand2);
                }
                else if (dynamic_cast<ir::type::IRFloatType*>(type) != nullptr || dynamic_cast<ir::type::IRDoubleType*>(
                    type) != nullptr)
                {
                    result = builder->CreateFDiv(operand1, operand2);
                }
                else
                {
                    throw std::runtime_error("unsupported type");
                }
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::MOD:
            {
                auto* type = irBinaryOperates->operand1->getType();
                if (const auto* integerType = dynamic_cast<ir::type::IRIntegerType*>(type))
                {
                    if (integerType->_unsigned) result = builder->CreateURem(operand1, operand2);
                    else result = builder->CreateSRem(operand1, operand2);
                }
                else if (dynamic_cast<ir::type::IRFloatType*>(type) != nullptr || dynamic_cast<ir::type::IRDoubleType*>(
                    type) != nullptr)
                {
                    result = builder->CreateFRem(operand1, operand2);
                }
                else
                {
                    throw std::runtime_error("unsupported type");
                }
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::AND:
            {
                result = builder->CreateAnd(operand1, operand2);
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::OR:
            {
                result = builder->CreateOr(operand1, operand2);
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::XOR:
            {
                result = builder->CreateXor(operand1, operand2);
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::SHL:
            {
                result = builder->CreateShl(operand1, operand2);
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::SHR:
            {
                result = builder->CreateLShr(operand1, operand2);
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::USHR:
            {
                result = builder->CreateAShr(operand1, operand2);
                break;
            }
        default:
            {
                throw std::runtime_error("unsupported operator");
            }
        }
        register2Value[irBinaryOperates->target] = result;
        return nullptr;
    }


    std::any LLVMIRGenerator::visitReturn(ir::instruction::IRReturn* irReturn, std::any additional)
    {
        if (irReturn->value == nullptr)
        {
            builder->CreateRetVoid();
        }
        else
        {
            visit(irReturn->value, additional);
            builder->CreateRet(std::any_cast<llvm::Value*>(stack.top()));
            stack.pop();
        }
        return nullptr;
    }

    std::any LLVMIRGenerator::visitRegister(ir::value::IRRegister* irRegister, std::any additional)
    {
        stack.emplace(register2Value[irRegister]);
        return nullptr;
    }

    std::any LLVMIRGenerator::visitIntegerConstant(ir::value::constant::IRIntegerConstant* irIntegerConstant,
                                                   std::any additional)
    {
        stack.push(std::make_any<llvm::Value*>(llvm::ConstantInt::get(
            *context, llvm::APInt(static_cast<uint32_t>(irIntegerConstant->type->size), irIntegerConstant->value,
                                  irIntegerConstant->type->_unsigned))));
        return nullptr;
    }


    std::any LLVMIRGenerator::visitIntegerType(ir::type::IRIntegerType* irIntegerType, std::any additional)
    {
        stack.push(std::make_any<llvm::Type*>(
            llvm::IntegerType::get(*context, static_cast<uint32_t>(irIntegerType->size))));
        return nullptr;
    }

    void compile(llvm::Module* module, std::string triple, std::string output)
    {
        std::string tmpFile = output + ".ll";
        module->setTargetTriple(llvm::Triple(triple));
        std::error_code EC;
        llvm::raw_fd_ostream Out(tmpFile, EC);
        if (EC)
        {
            throw std::runtime_error("Failed to open file: " + tmpFile);
        }
        module->print(Out, nullptr);
        Out.flush();

        clang::DiagnosticOptions DiagOpts;
        llvm::IntrusiveRefCntPtr DiagID(new clang::DiagnosticIDs());
        clang::DiagnosticsEngine Diags(DiagID, DiagOpts, new clang::TextDiagnosticPrinter(llvm::errs(), DiagOpts));

        std::string ClangPath = "clang";
        llvm::ErrorOr<std::string> ClangPathOrErr = llvm::sys::findProgramByName("clang");
        if (ClangPathOrErr)
        {
            ClangPath = ClangPathOrErr.get();
        }

        clang::driver::Driver Driver(ClangPath, triple, Diags);

        std::vector<std::string> args = {ClangPath, "-x", "ir", tmpFile};

        auto addArg = [&args](const std::string& flag, const std::string& value = "")
        {
            args.emplace_back(flag);
            if (!value.empty())
            {
                args.emplace_back(value);
            }
        };

        addArg("-o", output);

        std::vector<const char*> argsText;
        for (const auto& arg : args)
        {
            argsText.push_back(arg.c_str());
        }
        argsText.push_back(nullptr);

        const auto C = Driver.BuildCompilation(argsText);
        llvm::SmallVector<std::pair<int, const clang::driver::Command*>, 4> Failing;
        C->ExecuteJobs(C->getJobs(), Failing);

        if (llvm::sys::fs::exists(tmpFile))
        {
            if (std::error_code ec = llvm::sys::fs::remove(tmpFile))
            {
                llvm::errs() << "Failed to remove file: " << ec.message() << "\n";
            }
        }
    }
}
