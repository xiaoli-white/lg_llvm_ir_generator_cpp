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

    std::any LLVMIRGenerator::visitAssembly(ir::instruction::IRAssembly* irAssembly, std::any additional)
    {
        std::vector<llvm::Value*> operands(irAssembly->operands.size());
        std::vector<llvm::Type*> argTypes(irAssembly->operands.size());
        for (size_t i = 0; i < irAssembly->operands.size(); ++i)
        {
            visit(irAssembly->operands[i], additional);
            operands[i] = std::any_cast<llvm::Value*>(stack.top());
            stack.pop();
            argTypes[i] = operands[i]->getType();
        }
        auto* functionType = llvm::FunctionType::get(llvm::Type::getVoidTy(*context), argTypes, false);
        auto* inlineAsm = llvm::InlineAsm::get(functionType, irAssembly->assembly, irAssembly->constraints, false);
        builder->CreateCall(inlineAsm, operands);
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
                if (dynamic_cast<ir::type::IRIntegerType*>(irBinaryOperates->operand1->getType()))
                    result = builder->CreateAdd(operand1, operand2);
                else
                    result = builder->CreateFAdd(operand1, operand2);
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::SUB:
            {
                if (dynamic_cast<ir::type::IRIntegerType*>(irBinaryOperates->operand1->getType()))
                    result = builder->CreateSub(operand1, operand2);
                else
                    result = builder->CreateFSub(operand1, operand2);
                break;
            }
        case ir::instruction::IRBinaryOperates::Operator::MUL:
            {
                if (dynamic_cast<ir::type::IRIntegerType*>(irBinaryOperates->operand1->getType()))
                    result = builder->CreateMul(operand1, operand2);
                else
                    result = builder->CreateFMul(operand1, operand2);
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

    std::any LLVMIRGenerator::visitUnaryOperates(ir::instruction::IRUnaryOperates* irUnaryOperates, std::any additional)
    {
        visit(irUnaryOperates->operand, additional);
        auto* operand = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        llvm::Value* result;
        switch (irUnaryOperates->op)
        {
        case ir::instruction::IRUnaryOperates::Operator::INC:
            {
                visit(dynamic_cast<ir::type::IRPointerType*>(irUnaryOperates->operand->getType())->base, additional);
                auto* ty = std::any_cast<llvm::Type*>(stack.top());
                stack.pop();
                auto* tmp = builder->CreateLoad(ty, operand);
                auto* val = builder->CreateAdd(tmp, llvm::ConstantInt::get(ty, 1));
                result = builder->CreateStore(val, operand);
                break;
            }
        case ir::instruction::IRUnaryOperates::Operator::DEC:
            {
                visit(dynamic_cast<ir::type::IRPointerType*>(irUnaryOperates->operand->getType())->base, additional);
                auto* ty = std::any_cast<llvm::Type*>(stack.top());
                stack.pop();
                auto* tmp = builder->CreateLoad(ty, operand);
                auto* val = builder->CreateSub(tmp, llvm::ConstantInt::get(ty, 1));
                result = builder->CreateStore(val, operand);
                break;
            }
        case ir::instruction::IRUnaryOperates::Operator::NOT:
            {
                result = builder->CreateNot(operand);
                break;
            }
        case ir::instruction::IRUnaryOperates::Operator::NEG:
            {
                result = builder->CreateNeg(operand);
                break;
            }
        default:
            throw std::runtime_error(
                "unsupported operator: " + ir::instruction::IRUnaryOperates::operatorToString(irUnaryOperates->op));
        }
        register2Value[irUnaryOperates->target] = result;
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

    std::any LLVMIRGenerator::visitTypeCast(ir::instruction::IRTypeCast* irTypeCast, std::any additional)
    {
        visit(irTypeCast->source, additional);
        auto* source = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        visit(irTypeCast->targetType, additional);
        auto* targetType = std::any_cast<llvm::Type*>(stack.top());
        stack.pop();
        llvm::Instruction::CastOps op;
        switch (irTypeCast->kind)
        {
        case ir::instruction::IRTypeCast::Kind::ZEXT:
            op = llvm::Instruction::CastOps::ZExt;
            break;
        case ir::instruction::IRTypeCast::Kind::SEXT:
            op = llvm::Instruction::CastOps::SExt;
            break;
        case ir::instruction::IRTypeCast::Kind::TRUNC:
            op = llvm::Instruction::CastOps::Trunc;
            break;
        case ir::instruction::IRTypeCast::Kind::INTTOF:
            if (dynamic_cast<ir::type::IRIntegerType*>(irTypeCast->source->getType())->_unsigned)
                op = llvm::Instruction::CastOps::UIToFP;
            else
                op = llvm::Instruction::CastOps::SIToFP;
            break;
        case ir::instruction::IRTypeCast::Kind::FTOINT:
            if (dynamic_cast<ir::type::IRIntegerType*>(irTypeCast->targetType)->_unsigned)
                op = llvm::Instruction::CastOps::FPToUI;
            else
                op = llvm::Instruction::CastOps::FPToSI;
            break;
        case ir::instruction::IRTypeCast::Kind::INTTOPTR:
            op = llvm::Instruction::CastOps::IntToPtr;
            break;
        case ir::instruction::IRTypeCast::Kind::PTRTOINT:
            op = llvm::Instruction::CastOps::PtrToInt;
            break;
        case ir::instruction::IRTypeCast::Kind::PTRTOPTR:
            op = llvm::Instruction::CastOps::BitCast;
            break;
        case ir::instruction::IRTypeCast::Kind::FTRUNC:
            op = llvm::Instruction::CastOps::FPTrunc;
            break;
        case ir::instruction::IRTypeCast::Kind::BITCAST:
            op = llvm::Instruction::CastOps::BitCast;
            break;
        default:
            throw std::runtime_error(
                "Unsupported type cast kind: " + ir::instruction::IRTypeCast::kindToString(irTypeCast->kind));
        }
        auto* result = builder->CreateCast(op, source, targetType);
        register2Value[irTypeCast->target] = result;
        return nullptr;
    }

    std::any LLVMIRGenerator::visitPhi(ir::instruction::IRPhi* irPhi, std::any additional)
    {
        visit(irPhi->values.begin()->second->getType(), additional);
        auto* ty = std::any_cast<llvm::Type*>(stack.top());
        stack.pop();
        auto* phiInst = builder->CreatePHI(ty, irPhi->values.size());
        for (auto& [block, value] : irPhi->values)
        {
            visit(value, additional);
            auto* llvmVal = std::any_cast<llvm::Value*>(stack.top());
            stack.pop();
            phiInst->addIncoming(llvmVal, irBlock2LLVMBlock[block]);
        }
        register2Value[irPhi->target] = phiInst;
        return nullptr;
    }

    std::any LLVMIRGenerator::visitSwitch(ir::instruction::IRSwitch* irSwitch, std::any additional)
    {
        visit(irSwitch->value, additional);
        auto* val = std::any_cast<llvm::Value*>(stack.top());
        stack.pop();
        auto* switchInst = builder->CreateSwitch(val, irBlock2LLVMBlock[irSwitch->defaultCase],
                                                 irSwitch->cases.size());
        for (auto& [value, block] : irSwitch->cases)
        {
            visit(value, additional);
            auto* llvmVal = std::any_cast<llvm::Value*>(stack.top());
            stack.pop();
            auto* constantInt = llvm::cast<llvm::ConstantInt>(llvmVal);
            if (!constantInt)throw std::runtime_error("Switch case value is not an integer constant");
            switchInst->addCase(constantInt, irBlock2LLVMBlock[block]);
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

    std::any LLVMIRGenerator::visitFloatConstant(ir::value::constant::IRFloatConstant* irFloatConstant,
                                                 std::any additional)
    {
        stack.push(std::make_any<llvm::Value*>(llvm::ConstantFP::get(
            *context, llvm::APFloat(irFloatConstant->value))));
        return nullptr;
    }

    std::any LLVMIRGenerator::visitDoubleConstant(ir::value::constant::IRDoubleConstant* irDoubleConstant,
                                                  std::any additional)
    {
        stack.push(std::make_any<llvm::Value*>(
            llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context), irDoubleConstant->value)));
        return nullptr;
    }


    std::any LLVMIRGenerator::visitIntegerType(ir::type::IRIntegerType* irIntegerType, std::any additional)
    {
        stack.push(std::make_any<llvm::Type*>(
            llvm::IntegerType::get(*context, static_cast<uint32_t>(irIntegerType->size))));
        return nullptr;
    }

    std::any LLVMIRGenerator::visitFloatType(ir::type::IRFloatType* irFloatType, std::any additional)
    {
        stack.push(std::make_any<llvm::Type*>(llvm::Type::getFloatTy(*context)));
        return nullptr;
    }

    std::any LLVMIRGenerator::visitDoubleType(ir::type::IRDoubleType* irDoubleType, std::any additional)
    {
        stack.push(std::make_any<llvm::Type*>(llvm::Type::getDoubleTy(*context)));
        return nullptr;
    }

    std::any LLVMIRGenerator::visitVoidType(ir::type::IRVoidType* irVoidType, std::any additional)
    {
        stack.push(std::make_any<llvm::Type*>(llvm::Type::getVoidTy(*context)));
        return nullptr;
    }

    std::any LLVMIRGenerator::visitPointerType(ir::type::IRPointerType* irPointerType, std::any additional)
    {
        visit(irPointerType->base, additional);
        auto* base = std::any_cast<llvm::Type*>(stack.top());
        stack.pop();
        stack.push(std::make_any<llvm::Type*>(llvm::PointerType::get(base, 0)));
        return nullptr;
    }

    std::any LLVMIRGenerator::visitArrayType(ir::type::IRArrayType* irArrayType, std::any additional)
    {
        visit(irArrayType->base, additional);
        auto* base = std::any_cast<llvm::Type*>(stack.top());
        stack.pop();
        stack.push(std::make_any<llvm::Type*>(llvm::ArrayType::get(base, irArrayType->size)));
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
