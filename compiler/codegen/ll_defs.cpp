/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    ll_defs.h
 * authors: nvitya
 * created: 2026-02-19
 * brief:   LLVM defines, object
 */

#include "ll_defs.h"
#include "dqc.h"
#include "scf_dq.h"
#include "comp_options.h"
#include "otype_int.h"
#include "otype_enum.h"

LlContext      ll_ctx;
LlBuilder      ll_builder(ll_ctx, llvm::ConstantFolder(),
    llvm::IRBuilderCallbackInserter([](llvm::Instruction * inst) {
      if (g_compiler) g_compiler->RecordSignalMemoryAccess(inst);
    }));
LlModule *     ll_module;

vector<SLoopContext>   ll_loop_stack;

LlDiBuilder *          di_builder = nullptr;
LlDiUnit *             di_unit = nullptr;
LlDiFile *             di_main_file = nullptr;

void ll_defs_init()
{
  ll_module = new llvm::Module("dq", ll_ctx);
  di_builder = new LlDiBuilder(*ll_module);
}

void ll_init_debug_info()
{
  if (not g_opt.dbg_info)
  {
    return;
  }

  ll_module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
  ll_module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 4);

  OScFile * pfile = g_compiler->scf->curfile;
  if (!pfile->di_file)
  {
    pfile->di_file = di_builder->createFile(pfile->name, ExtractFilePath(pfile->fullpath));
  }
  di_main_file = pfile->di_file;
  // GDB has no DQ language frontend; use its C++ expression parser so that
  // canonical DQ module scopes can be addressed with the `::` separator.
  di_unit = di_builder->createCompileUnit(
      llvm::dwarf::DW_LANG_C_plus_plus, di_main_file, "dqc", false, "", 0);
}

static LlValue * ResizeIntValue(LlValue * value, LlType * dst, bool issigned, const char * prefix)
{
  if (value->getType() == dst)
  {
    return value;
  }
  if (!value->getType()->isIntegerTy())
  {
    return value;
  }
  unsigned srcbits = value->getType()->getIntegerBitWidth();
  unsigned dstbits = static_cast<llvm::IntegerType *>(dst)->getBitWidth();
  if (srcbits < dstbits)
  {
    string name = string(prefix) + ".ext";
    return issigned ? ll_builder.CreateSExt(value, dst, name)
                    : ll_builder.CreateZExt(value, dst, name);
  }
  if (srcbits > dstbits)
  {
    return ll_builder.CreateTrunc(value, dst, string(prefix) + ".trunc");
  }
  return value;
}

static bool IsSignedIntOrEnumType(OType * srctype)
{
  OType * resolved = srctype ? srctype->ResolveAlias() : nullptr;
  if (auto * enumtype = dynamic_cast<OTypeEnum *>(resolved))
  {
    resolved = enumtype->storage_type;
  }
  if (auto * inttype = dynamic_cast<OTypeInt *>(resolved))
  {
    return inttype->issigned;
  }
  return true;
}

LlValue * ToNativeInt(LlValue * value, bool issigned)
{
  return ResizeIntValue(value, g_builtins->type_int->GetLlType(), issigned, "int");
}

LlValue * ToNativeInt(LlValue * value, OType * srctype)
{
  return ToNativeInt(value, IsSignedIntOrEnumType(srctype));
}

LlValue * ToNativeUInt(LlValue * value)
{
  return ResizeIntValue(value, g_builtins->type_uint->GetLlType(), false, "uint");
}

LlValue * ToCharValue(LlValue * value)
{
  return ResizeIntValue(value, g_builtins->type_char->GetLlType(), false, "ch");
}
