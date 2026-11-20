// SPDX-License-Identifier: MIT
#include "otype_varargs.h"
#include "dqc.h"
#include "dqc_ast.h"
#include "scope_builtins.h"

// Private compiler/RTL ABI, shared with rtl/varargs.dq. Bit zero indicates
// a second payload word. Kind zero is reserved for an invalid element.
enum EVarArgKind : uint8_t
{
  VA_SINT = 2, VA_UINT = 4, VA_FLOAT32 = 6, VA_FLOAT64 = 8,
  VA_BOOL = 10, VA_CHAR = 12, VA_WCHAR = 14, VA_STRVIEW = 16,
  VA_PCHAR = 18, VA_POINTER = 20
};

OTypeVarArgs::OTypeVarArgs() : OType("varargs", TK_VARARGS)
{
  bytesize = 2 * TARGET_PTRSIZE;
  alignsize = TARGET_PTRSIZE;
}

LlType * OTypeVarArgs::CreateLlType()
{
  LlType * ptr = g_builtins->type_pointer->GetLlType();
  return llvm::StructType::get(ll_ctx, {ptr, ptr});
}

LlDiType * OTypeVarArgs::CreateDiType()
{
  return di_builder->createStructType(nullptr, name, nullptr, 0, bytesize * 8,
      alignsize * 8, llvm::DINode::FlagZero, nullptr, di_builder->getOrCreateArray({}));
}

uint8_t OTypeVarArgs::Tag(OExpr * expr)
{
  OType * type = expr->ResolvedType();
  if (!type) return 0;
  bool wide = type->bytesize > TARGET_PTRSIZE;
  switch (type->kind)
  {
    case TK_INT: return (static_cast<OTypeInt *>(type)->issigned ? VA_SINT : VA_UINT) | wide;
    case TK_FLOAT: return (type->bytesize == 4 ? VA_FLOAT32 : VA_FLOAT64) | wide;
    case TK_BOOL: return VA_BOOL;
    case TK_CHAR: return type == g_builtins->type_char ? VA_CHAR : VA_WCHAR;
    case TK_WCHAR: return VA_WCHAR;
    case TK_POINTER:
      if (dynamic_cast<OEmbStrLit *>(expr)) return VA_STRVIEW | 1;
      return IsCCharPointerType(type) ? VA_PCHAR : VA_POINTER;
    case TK_ROSTR: case TK_STRSLICE: case TK_EMBSTR: case TK_DYNSTR:
      return VA_STRVIEW | 1;
    default: return 0;
  }
}

int OTypeVarArgs::AnalyzeConversion(OExpr ** rexpr, uint32_t flags, bool convert)
{
  OExpr * src = *rexpr;
  if (!(flags & EXPCF_ALLOW_VARARGS)) return ConversionError(src->ResolvedType(), flags);
  if (src->ResolvedType() == this) return 0;

  auto * list = dynamic_cast<OArrayLit *>(src);
  vector<OExpr *> singleton;
  if (!list) singleton.push_back(src);
  auto & elements = list ? list->elements : singleton;
  if (elements.size() > 255)
  {
    if (flags & EXPCF_GENERATE_ERRORS)
      g_compiler->ErrorTxt(DQERR_NOT_SUPPORTED, "varargs supports at most 255 elements");
    return -1;
  }
  for (size_t i = 0; i < elements.size(); ++i)
  {
    OExpr * element = elements[i];
    if (!Tag(element))
    {
      if (flags & EXPCF_GENERATE_ERRORS)
        g_compiler->ErrorTxt(DQERR_TYPEMISM, format("unsupported varargs element {} of type {}",
            i, element->ptype ? element->ptype->name : "unresolved"));
      return -1;
    }
    if (convert && element->ResolvedType()->IsString())
    {
      bool managed = TK_DYNSTR == element->ResolvedType()->kind;
      if (!(managed ? EnsureDynStringRtlUseForStringTypes() : EnsureStrFuncRtlUse())) return -1;
    }
  }
  if (convert)
  {
    *rexpr = new OVarArgsPackExpr(this, std::move(elements));
    delete list;
  }
  return 1;
}

OVarArgsPackExpr::OVarArgsPackExpr(OType * type, vector<OExpr *> && values)
  : elements(std::move(values))
{
  ptype = type;
  auto retain = [&](OExpr * source)
  {
    auto * owner = new OCachedValueExpr(source);
    owners.push_back(owner);
    return owner;
  };
  for (OExpr *& elem : elements)
  {
    // Keep the evaluated manager, including the old value of a rebound lvalue.
    if (TK_DYNSTR == elem->ResolvedType()->kind)
    {
      elem = retain(elem);
    }
    else if (auto * borrow = dynamic_cast<OTextBorrowExpr *>(elem);
             borrow && TK_DYNSTR == borrow->source->ResolvedType()->kind)
    {
      borrow->source = retain(borrow->source);
    }
    else if (auto * meta = dynamic_cast<OStringMetaFieldExpr *>(elem); meta && meta->NeedsCallCleanup())
    {
      meta->receiver = retain(meta->receiver);
    }
  }
}

void OVarArgsPackExpr::PrepareOwners(vector<OExpr *> & cleanups)
{
  for (auto * owner : owners)
  {
    owner->PrepareStorage();
    cleanups.push_back(owner);
  }
}

void OVarArgsPackExpr::FoldChildren()
{
  for (OExpr *& elem : elements) OExpr::FoldTree(&elem);
}

OVarArgsPackExpr::~OVarArgsPackExpr()
{
  for (OExpr * elem : elements) delete elem;
}

LlValue * OVarArgsPackExpr::Descriptor(const vector<uint8_t> & tags)
{
  string name = ".dqva.desc.";
  for (uint8_t tag : tags) name += format("{:02x}", tag);
  if (auto * desc = ll_module->getNamedGlobal(name)) return desc;
  auto * bytes = llvm::ConstantDataArray::get(ll_ctx, tags);
  return new llvm::GlobalVariable(*ll_module, bytes->getType(), true,
      llvm::GlobalValue::PrivateLinkage, bytes, name);
}

LlValue * OVarArgsPackExpr::Generate(OScope * scope)
{
  LlType * word = g_builtins->native_uint->GetLlType();
  vector<uint8_t> tags = {static_cast<uint8_t>(elements.size())};
  unsigned words = 0;
  for (OExpr * elem : elements)
  {
    uint8_t tag = OTypeVarArgs::Tag(elem);
    tags.push_back(tag);
    words += 1 + (tag & 1);
  }

  // Buffer a constant prefix until the first live value decides whether the
  // whole payload can stay in flash. Live values go straight to their slots.
  vector<LlConst *> constants;
  LlValue * data = nullptr;
  unsigned offset = 0;
  auto store = [&](LlValue * value)
  {
    auto * addr = ll_builder.CreateConstGEP1_32(word, data, offset);
    ll_builder.CreateAlignedStore(value, addr, llvm::Align(TARGET_PTRSIZE));
    offset += ll_module->getDataLayout().getTypeStoreSize(value->getType()) / TARGET_PTRSIZE;
  };
  auto append = [&](LlValue * value)
  {
    if (!data)
    {
      if (auto * constant = llvm::dyn_cast<LlConst>(value))
      {
        constants.push_back(constant);
        return;
      }
      data = CreateEntryBlockAlloca(llvm::ArrayType::get(word, words), nullptr, "varargs.payload");
      for (LlConst * constant : constants) store(constant);
    }
    store(value);
  };

  for (size_t i = 0; i < elements.size(); ++i)
  {
    OExpr * elem = elements[i];
    uint8_t tag = tags[i + 1];
    if (auto * literal = dynamic_cast<OEmbStrLit *>(elem))
    {
      append(literal->Generate(scope));
      append(llvm::ConstantInt::get(word, literal->value.size()));
    }
    else if ((tag & ~1) == VA_STRVIEW)
    {
      LlValue * view = GenerateTextInfoValue(scope, elem);
      append(ll_builder.CreateExtractValue(view, 0));
      append(ToNativeUInt(ll_builder.CreateExtractValue(view, 1)));
    }
    else
    {
      LlValue * value = elem->Generate(scope);
      if (value->getType()->isIntegerTy() && !(tag & 1))
        value = ToNativeInt(value, tag == VA_SINT);
      if (value->getType()->isFloatTy() && TARGET_PTRSIZE == 8)
      {
        // Float bytes start at the first address of the word on both endians.
        value = ToNativeUInt(ll_builder.CreateBitCast(value, LlType::getInt32Ty(ll_ctx)));
        if (ll_module->getDataLayout().isBigEndian()) value = ll_builder.CreateShl(value, 32);
      }
      append(value);
    }
  }

  if (!words)
    data = llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0));
  else if (!data)
  {
    auto * payload = llvm::ConstantStruct::getAnon(ll_ctx, constants, true);
    auto * global = new llvm::GlobalVariable(*ll_module, payload->getType(), true,
        llvm::GlobalValue::PrivateLinkage, payload, ".dqva.payload");
    global->setAlignment(llvm::Align(TARGET_PTRSIZE));
    global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    data = global;
  }
  LlValue * pack = llvm::UndefValue::get(ptype->GetLlType());
  pack = ll_builder.CreateInsertValue(pack, data, 0);
  return ll_builder.CreateInsertValue(pack, Descriptor(tags), 1);
}

OVarArgsFieldExpr::OVarArgsFieldExpr(OExpr * pack, const string & name)
  : source(pack), field(name == "data" ? 0 : 1)
{
  if (name == "length") ptype = g_builtins->native_uint;
  else if (name == "data") ptype = g_builtins->native_uint->GetPointerType();
  else ptype = g_builtins->type_uint8->GetPointerType();
}

LlValue * OVarArgsFieldExpr::Generate(OScope * scope)
{
  LlValue * ptr = ll_builder.CreateExtractValue(source->Generate(scope), field);
  if (ptype == g_builtins->native_uint)
    return ToNativeUInt(ll_builder.CreateLoad(LlType::getInt8Ty(ll_ctx), ptr));
  return ptr;
}
