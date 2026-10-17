/*
 * Copyright (c) 2026 Viktor Nagy and DQ contributors
 * SPDX-License-Identifier: MIT
 */

#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/InlineAsm.h>

#include "dqc.h"
#include "dqc_codegen.h"

// LLVM loads and stores cannot have unwind edges. An unwindable inline asm
// emits the same memory instruction, with an exception-table entry rather
// than a runtime check or an extra function call.
class OSignalMemoryAccess
{
  LlBasicBlock * cleanup;

public:
  explicit OSignalMemoryAccess(LlBasicBlock * acleanup) : cleanup(acleanup) {}

  LlValue * Generate(LlType * type, LlValue * address, LlValue * value = nullptr)
  {
    if (type->isAggregateType())
    {
      LlValue * result = llvm::UndefValue::get(type);
      auto * structure = llvm::dyn_cast<llvm::StructType>(type);
      unsigned count = structure ? structure->getNumElements()
          : llvm::cast<llvm::ArrayType>(type)->getNumElements();
      for (unsigned i = 0; i < count; ++i)
      {
        LlType * element_type = structure ? structure->getElementType(i)
            : llvm::cast<llvm::ArrayType>(type)->getElementType();
        LlValue * element_address = ll_builder.CreateGEP(type, address,
            {ll_builder.getInt32(0), ll_builder.getInt32(i)});
        LlValue * element_value = value ? ll_builder.CreateExtractValue(value, {i}) : nullptr;
        LlValue * element = Generate(element_type, element_address, element_value);
        if (!value) result = ll_builder.CreateInsertValue(result, element, {i});
      }
      return value ? nullptr : result;
    }

    bool is_bool = type->isIntegerTy(1);
    LlType * memory_type = is_bool ? ll_builder.getInt8Ty() : type;
    if (value && is_bool) value = ll_builder.CreateZExt(value, memory_type);
    string instruction = type->isFloatTy() ? "movss" : type->isDoubleTy() ? "movsd" : "mov";
    string reg = type->isFloatingPointTy() ? "x" : "r";
    string constraints = value ? "=*m," + reg : "=" + reg + ",*m";
    vector<LlValue *> args = {address};
    vector<LlType *> argtypes = {address->getType()};
    if (value)
    {
      args.push_back(value);
      argtypes.push_back(memory_type);
    }
    LlFuncType * signature = LlFuncType::get(value ? ll_builder.getVoidTy() : memory_type,
        argtypes, false);
    auto * assembly = llvm::InlineAsm::get(signature, instruction + " $0, $1",
        constraints, true, false, llvm::InlineAsm::AD_Intel, true);
    llvm::CallBase * call;
    if (cleanup)
    {
      LlFunction * function = ll_builder.GetInsertBlock()->getParent();
      g_compiler->EnsurePersonalityFn(function);
      auto * next = LlBasicBlock::Create(ll_ctx, "memory.cont", function);
      call = ll_builder.CreateInvoke(signature, assembly, next, cleanup, args);
      ll_builder.SetInsertPoint(next);
    }
    else
    {
      call = ll_builder.CreateCall(signature, assembly, args);
    }
    call->addParamAttr(0, llvm::Attribute::get(ll_ctx, llvm::Attribute::ElementType, memory_type));
    if (value) return nullptr;
    return is_bool ? ll_builder.CreateTrunc(call, type) : call;
  }
};

void ODqCompCodegen::RecordSignalMemoryAccess(llvm::Instruction * inst)
{
  if (!ll_codegen_scope || !g_opt.exceptions || !g_opt.target.IsLinux() || !g_opt.target.IsX64()) return;
  LlValue * address = nullptr;
  if (auto * load = llvm::dyn_cast<llvm::LoadInst>(inst)) address = load->getPointerOperand();
  if (auto * store = llvm::dyn_cast<llvm::StoreInst>(inst)) address = store->getPointerOperand();
  if (!address) return;
  LlValue * storage = llvm::getUnderlyingObject(address);
  if (llvm::isa<llvm::AllocaInst>(storage) || llvm::isa<llvm::GlobalVariable>(storage)) return;
  LlBasicBlock * cleanup = ll_codegen_scope->GetExceptionCleanupBB();
  // Collection operations can emit a helper function while generating their
  // caller. That helper unwinds to its caller, not to the caller's local pad.
  if (cleanup && cleanup->getParent() != inst->getFunction()) cleanup = nullptr;
  ll_signal_memory_accesses.push_back({inst, cleanup});
}

void ODqCompCodegen::GenerateSignalMemoryAccesses()
{
  for (auto [inst, cleanup] : ll_signal_memory_accesses)
  {
    LlBasicBlock * block = inst->getParent();
    LlBasicBlock * continuation = nullptr;
    if (cleanup)
    {
      continuation = block->splitBasicBlock(inst->getIterator(), "memory.done");
      block->getTerminator()->eraseFromParent();
      ll_builder.SetInsertPoint(block);
    }
    else
    {
      ll_builder.SetInsertPoint(inst);
    }
    ll_builder.SetCurrentDebugLocation(inst->getDebugLoc());
    OSignalMemoryAccess access(cleanup);
    if (auto * load = llvm::dyn_cast<llvm::LoadInst>(inst))
    {
      LlValue * result = access.Generate(load->getType(), load->getPointerOperand());
      load->replaceAllUsesWith(result);
    }
    else if (auto * store = llvm::dyn_cast<llvm::StoreInst>(inst))
    {
      access.Generate(store->getValueOperand()->getType(), store->getPointerOperand(), store->getValueOperand());
    }
    inst->eraseFromParent();
    if (continuation) ll_builder.CreateBr(continuation);
  }
  ll_signal_memory_accesses.clear();
}
