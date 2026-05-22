/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_object.h
 * authors: nvitya
 * created: 2026-05-22
 * brief:   object type and value symbol helpers
 */

#pragma once

#include "symbols.h"

class OValSymFunc;

class OTypeObject : public OCompoundType
{
private:
  using  super = OCompoundType;

public:
  OTypeObject * base_type = nullptr;
  bool          is_polymorphic = false;
  bool          is_abstract = false;
  uint32_t      vtable_field_index = 0;
  LlValue *     ll_vtable = nullptr;
  vector<OValSymFunc *> virtual_methods;
  vector<OValSymFunc *> constructors;
  OValSymFunc * destructor = nullptr;

  OTypeObject(const string name, OScope * aparent_scope)
  :
    super(name, aparent_scope)
  {
    member_scope.vs_lookup_parent = false;
  }

  bool IsObject() const override { return true; }
  OValSym * CreateValSym(OScPosition & apos, const string aname) override;
  bool IsSameOrDerivedFrom(OCompoundType * abase) const override;
  OValSymFunc * FindSpecialMethod(EObjectSpecFuncKind akind, size_t auser_arg_count = size_t(-1)) const;
  OValSymFunc * FindConstructorForArgs(const vector<OExpr *> & aargs, bool * rambiguous = nullptr) const;
  OValSym * FindObjectMemberSymbol(const string & aname, OCompoundType ** rdecl_type = nullptr) const;
  int FindObjectFieldIndex(const string & aname, OCompoundType ** rdecl_type = nullptr) const;
  OValSymFunc * FindVirtualBaseMethod(OValSymFunc * afunc, OCompoundType ** rdecl_type = nullptr) const;
  int FindVirtualSlot(OValSymFunc * afunc) const;
  void UpdateObjectInheritanceFlags();
  void GenVTableGlobal(bool apublic);
  void GenerateVTableStore(LlValue * ll_object_addr);

  void        EnsureLayout() override;
  LlType *    CreateLlType() override;
  LlDiType *  CreateDiType() override;
  bool        WriteDqmIfDecl(ODqmIfWriter & writer) override;
};

class OVsObject : public OValSym
{
private:
  using  super = OValSym;

public:
  EObjectStorageKind object_storage = OSK_OBJECT_REF;
  vector<OExpr *>    object_ctor_args;
  bool               object_ctor_call_at_decl = false;

  OVsObject(OScPosition & apos, const string aname, OType * atype,
            EValSymKind akind = VSK_VARIABLE, EObjectStorageKind astorage = OSK_OBJECT_REF)
  :
    super(apos, aname, atype, akind),
    object_storage(astorage)
  {
  }

  ~OVsObject() override;

  EObjectStorageKind ObjectStorage() const { return object_storage; }
  void SetObjectStorage(EObjectStorageKind astorage) { object_storage = astorage; }
  bool IsObjectReference() const { return (OSK_OBJECT_REF == object_storage); }
  bool IsFixedObjectStorage() const { return (OSK_OBJECT_FIXED == object_storage); }
  bool ObjectCtorCallAtDecl() const { return object_ctor_call_at_decl; }
  void SetObjectCtorCallAtDecl(bool acall) { object_ctor_call_at_decl = acall; }
  vector<OExpr *> & ObjectCtorArgs() { return object_ctor_args; }
  const vector<OExpr *> & ObjectCtorArgs() const { return object_ctor_args; }
  void SetObjectCtorArgs(vector<OExpr *> aargs);
  OType * GetStorageType() const override;

  OTypeObject * ObjectType() const;
  OValSymFunc * FindConstructor() const;
  OValSymFunc * FindDestructor() const;
  void GenerateConstructorCall(OScope * scope, LlValue * ll_object_addr) const;
  void GenerateDestructorCall(LlValue * ll_object_addr) const;
};
