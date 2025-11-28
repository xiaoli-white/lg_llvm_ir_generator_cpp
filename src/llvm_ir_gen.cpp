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
        for (const auto& global : module->globals | std::views::values)
        {
            visit(global->type, additional);
            auto* type = std::any_cast<llvm::Type*>(stack.top());
            stack.pop();
            irGlobalVariable2LLVMGlobalVariable[global] = new llvm::GlobalVariable(
                *llvmModule,
                type,
                global->isConstant,
                llvm::GlobalValue::ExternalLinkage,
                nullptr,
                global->name
            );
        }
        for (const auto& global : module->globals | std::views::values)
        {
            visit(global, additional);
        }
        for (const auto& func : module->functions | std::views::values)
        {
            visit(func->returnType, additional);
            const auto returnType = std::any_cast<llvm::Type*>(stack.top());
            stack.pop();
            std::vector<llvm::Type*> args;
            for (auto& arg : func->args)
            {
                visit(arg, additional);
                args.push_back(std::any_cast<llvm::Type*>(stack.top()));
                stack.pop();
            }
            const auto functionType = llvm::FunctionType::get(returnType, args, false);
            llvm::Function* llvmFunction = llvm::Function::Create(
                functionType,
                llvm::Function::ExternalLinkage,
                func->name,
                llvmModule
            );
            for (auto& arg : llvmFunction->args())arg.setName(func->args[arg.getArgNo()]->name);
        }
        for (const auto& func : module->functions | std::views::values)
        {
            visit(func, additional);
        }
        return nullptr;
    }

    std::any LLVMIRGenerator::visitGlobalVariable(ir::base::IRGlobalVariable* irGlobalVariable, std::any additional)
    {
        visit(irGlobalVariable->initializer, additional);
        auto* value = std::any_cast<llvm::Value*>(stack.top());
        auto* initializer = llvm::dyn_cast<llvm::Constant>(value);
        if (initializer == nullptr) throw std::runtime_error("unsupported type");
        irGlobalVariable2LLVMGlobalVariable[irGlobalVariable]->setInitializer(initializer);
        return nullptr;
    }


    std::any LLVMIRGenerator::visitFunction(ir::function::IRFunction* irFunction, std::any additional)
    {
        currentFunction = llvmModule->getFunction(irFunction->name);
        for (const auto& block : irFunction->cfg->basicBlocks | std::views::values)
        {
            llvm::BasicBlock* llvmBlock = llvm::BasicBlock::Create(*context, block->name, currentFunction);
            irBlock2LLVMBlock[block] = llvmBlock;
        }
        for (const auto& block : irFunction->cfg->basicBlocks | std::views::values)
        {
            builder->SetInsertPoint(irBlock2LLVMBlock[block]);
            for (const auto& instruction : block->instructions)visit(instruction, additional);
        }
        irBlock2LLVMBlock.clear();
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

    std::any LLVMIRGenerator::visitGetElementPointer(ir::instruction::IRGetElementPointer* irGetElementPointer,
                                                     std::any additional)
    {
        visit(dynamic_cast<ir::type::IRPointerType*>(irGetElementPointer->pointer->getType())->base, additional);
        auto* type = std::any_cast<llvm::Type*>(stack.top());
        stack.pop();
        visit(irGetElementPointer->pointer, additional);
        auto* ptr = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        std::vector<llvm::Value*> indices(irGetElementPointer->indices.size());
        for (const auto& index : irGetElementPointer->indices)
        {
            visit(index, additional);
            auto* indexValue = std::any_cast<llvm::Value*>(stack.top());
            stack.pop();
            indices.push_back(indexValue);
        }
        auto* result = builder->CreateGEP(type, ptr, std::move(indices));
        register2Value[irGetElementPointer->target] = result;
        return nullptr;
    }

    std::any LLVMIRGenerator::visitCompare(ir::instruction::IRCompare* irCompare, std::any additional)
    {
        visit(irCompare->operand1, additional);
        auto* operand1 = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        visit(irCompare->operand2, additional);
        auto* operand2 = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        bool isInteger;
        bool isUnsigned;
        if (const auto* integerType = dynamic_cast<ir::type::IRIntegerType*>(irCompare->operand1->getType()))
        {
            isInteger = true;
            isUnsigned = integerType->_unsigned;
        }
        else
        {
            isInteger = false;
            isUnsigned = false;
        }
        llvm::CmpInst::Predicate predicate;
        switch (irCompare->condition)
        {
        case ir::base::IRCondition::E:
            {
                if (isInteger)
                    predicate = llvm::CmpInst::Predicate::ICMP_EQ;
                else
                    predicate = llvm::CmpInst::Predicate::FCMP_OEQ;
                break;
            }
        case ir::base::IRCondition::NE:
            {
                if (isInteger)
                    predicate = llvm::CmpInst::Predicate::ICMP_NE;
                else
                    predicate = llvm::CmpInst::Predicate::FCMP_ONE;
                break;
            }
        case ir::base::IRCondition::L:
            {
                if (isInteger)
                    if (isUnsigned) predicate = llvm::CmpInst::Predicate::ICMP_ULT;
                    else predicate = llvm::CmpInst::Predicate::ICMP_SLT;
                else
                    predicate = llvm::CmpInst::Predicate::FCMP_ULT;
                break;
            }
        case ir::base::IRCondition::LE:
            {
                if (isInteger)
                    if (isUnsigned) predicate = llvm::CmpInst::Predicate::ICMP_ULE;
                    else predicate = llvm::CmpInst::Predicate::ICMP_SLE;
                else
                    predicate = llvm::CmpInst::Predicate::FCMP_ULE;
                break;
            }
        case ir::base::IRCondition::G:
            {
                if (isInteger)
                    if (isUnsigned) predicate = llvm::CmpInst::Predicate::ICMP_UGT;
                    else predicate = llvm::CmpInst::Predicate::ICMP_SGT;
                else
                    predicate = llvm::CmpInst::Predicate::FCMP_UGT;
                break;
            }
        case ir::base::IRCondition::GE:
            {
                if (isInteger)
                    if (isUnsigned) predicate = llvm::CmpInst::Predicate::ICMP_UGE;
                    else predicate = llvm::CmpInst::Predicate::ICMP_SGE;
                else
                    predicate = llvm::CmpInst::Predicate::FCMP_UGE;
                break;
            }
        default:
            {
                throw std::runtime_error("unsupported condition: " + ir::base::conditionToString(irCompare->condition));
            }
        }
        auto* result = builder->CreateCmp(predicate, operand1, operand2);
        register2Value[irCompare->target] = result;
        return nullptr;
    }

    std::any LLVMIRGenerator::visitConditionalJump(ir::instruction::IRConditionalJump* irConditionalJump,
                                                   std::any additional)
    {
        visit(irConditionalJump->operand1, additional);
        auto* operand1 = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        llvm::Value* cond;
        if (irConditionalJump->operand2 != nullptr)
        {
            visit(irConditionalJump->operand2, additional);
            auto* operand2 = std::any_cast<llvm::Value*>(stack.top());
            stack.pop();
            bool isInteger;
            bool isUnsigned;
            if (const auto* integerType = dynamic_cast<ir::type::IRIntegerType*>(irConditionalJump->operand1->
                getType()))
            {
                isInteger = true;
                isUnsigned = integerType->_unsigned;
            }
            else
            {
                isInteger = false;
                isUnsigned = false;
            }
            llvm::CmpInst::Predicate predicate;
            switch (irConditionalJump->condition)
            {
            case ir::base::IRCondition::E:
                {
                    if (isInteger)
                        predicate = llvm::CmpInst::Predicate::ICMP_EQ;
                    else
                        predicate = llvm::CmpInst::Predicate::FCMP_OEQ;
                    break;
                }
            case ir::base::IRCondition::NE:
                {
                    if (isInteger)
                        predicate = llvm::CmpInst::Predicate::ICMP_NE;
                    else
                        predicate = llvm::CmpInst::Predicate::FCMP_ONE;
                    break;
                }
            case ir::base::IRCondition::L:
                {
                    if (isInteger)
                        if (isUnsigned) predicate = llvm::CmpInst::Predicate::ICMP_ULT;
                        else predicate = llvm::CmpInst::Predicate::ICMP_SLT;
                    else
                        predicate = llvm::CmpInst::Predicate::FCMP_ULT;
                    break;
                }
            case ir::base::IRCondition::LE:
                {
                    if (isInteger)
                        if (isUnsigned) predicate = llvm::CmpInst::Predicate::ICMP_ULE;
                        else predicate = llvm::CmpInst::Predicate::ICMP_SLE;
                    else
                        predicate = llvm::CmpInst::Predicate::FCMP_ULE;
                    break;
                }
            case ir::base::IRCondition::G:
                {
                    if (isInteger)
                        if (isUnsigned) predicate = llvm::CmpInst::Predicate::ICMP_UGT;
                        else predicate = llvm::CmpInst::Predicate::ICMP_SGT;
                    else
                        predicate = llvm::CmpInst::Predicate::FCMP_UGT;
                    break;
                }
            case ir::base::IRCondition::GE:
                {
                    if (isInteger)
                        if (isUnsigned) predicate = llvm::CmpInst::Predicate::ICMP_UGE;
                        else predicate = llvm::CmpInst::Predicate::ICMP_SGE;
                    else
                        predicate = llvm::CmpInst::Predicate::FCMP_UGE;
                    break;
                }
            default:
                {
                    throw std::runtime_error(
                        "unsupported condition: " + ir::base::conditionToString(irConditionalJump->condition));
                }
            }
            cond = builder->CreateCmp(predicate, operand1, operand2);
        }
        else
        {
            if (irConditionalJump->condition == ir::base::IRCondition::IF_TRUE)
            {
                cond = operand1;
            }
            else if (irConditionalJump->condition == ir::base::IRCondition::IF_FALSE)
            {
                cond = builder->CreateNot(operand1);
            }
            else
            {
                throw std::runtime_error(
                    "unsupported condition: " + ir::base::conditionToString(irConditionalJump->condition));
            }
        }
        builder->CreateCondBr(cond, irBlock2LLVMBlock[irConditionalJump->target],
                              builder->GetInsertBlock()->getNextNode());
        return nullptr;
    }


    std::any LLVMIRGenerator::visitGoto(ir::instruction::IRGoto* irGoto, std::any additional)
    {
        builder->CreateBr(irBlock2LLVMBlock[irGoto->target]);
        return nullptr;
    }

    std::any LLVMIRGenerator::visitInvoke(ir::instruction::IRInvoke* irInvoke, std::any additional)
    {
        visit(irInvoke->func->getType(), additional);
        auto* ty = std::any_cast<llvm::Type*>(stack.top());
        stack.pop();
        auto* funcType = llvm::cast<llvm::FunctionType>(ty);
        visit(irInvoke->func, additional);
        auto* func = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        std::vector<llvm::Value*> args;
        for (auto* arg : irInvoke->arguments)
        {
            visit(arg, additional);
            args.push_back(std::any_cast<llvm::Value*>(stack.top()));
            stack.pop();
        }
        auto* result = builder->CreateCall(funcType, func, args);
        if (irInvoke->target != nullptr)
        {
            register2Value[irInvoke->target] = result;
        }
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

    std::any LLVMIRGenerator::visitLoad(ir::instruction::IRLoad* irLoad, std::any additional)
    {
        visit(dynamic_cast<ir::type::IRPointerType*>(irLoad->ptr->getType())->base, additional);
        auto* ty = std::any_cast<llvm::Type*>(stack.top());
        stack.pop();
        visit(irLoad->ptr, additional);
        auto* ptr = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        auto* result = builder->CreateLoad(ty, ptr);
        register2Value[irLoad->target] = result;
        return nullptr;
    }

    std::any LLVMIRGenerator::visitStore(ir::instruction::IRStore* irStore, std::any additional)
    {
        visit(irStore->ptr, additional);
        auto* ptr = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        visit(irStore->value, additional);
        auto* value = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        builder->CreateStore(value, ptr);
        return nullptr;
    }

    std::any LLVMIRGenerator::visitNop(ir::instruction::IRNop* irNop, std::any additional)
    {
        return nullptr;
    }

    std::any LLVMIRGenerator::visitSetRegister(ir::instruction::IRSetRegister* irSetRegister, std::any additional)
    {
        visit(irSetRegister->value, additional);
        register2Value[irSetRegister->target] = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        return nullptr;
    }

    std::any LLVMIRGenerator::visitStackAllocate(ir::instruction::IRStackAllocate* irStackAllocate, std::any additional)
    {
        visit(irStackAllocate->type, additional);
        auto* ty = std::any_cast<llvm::Type*>(stack.top());
        stack.pop();
        llvm::Value* size;
        if (irStackAllocate->size != nullptr)
        {
            visit(irStackAllocate->size, additional);
            size = std::any_cast<llvm::Value*>(stack.top());
            stack.pop();
        }
        else
        {
            size = nullptr;
        }
        auto* result = builder->CreateAlloca(ty, size);
        register2Value[irStackAllocate->target] = result;
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
