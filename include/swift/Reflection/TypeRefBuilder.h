//===--- TypeRefBuilder.h - Swift Type Reference Builder --------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// Implements utilities for constructing TypeRefs and looking up field and
// enum case types.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_REFLECTION_TYPEREFBUILDER_H
#define SWIFT_REFLECTION_TYPEREFBUILDER_H

#include "swift/Remote/MetadataReader.h"
#include "swift/Reflection/Records.h"
#include "swift/Reflection/TypeLowering.h"
#include "swift/Reflection/TypeRef.h"
#include "llvm/ADT/Optional.h"

#include <iostream>
#include <vector>
#include <unordered_map>

class NodePointer;

namespace swift {
namespace reflection {

template <typename Iterator>
class ReflectionSection {
  using const_iterator = Iterator;
  const void * Begin;
  const void * End;

public:
  ReflectionSection(const void * Begin,
                    const void * End)
  : Begin(Begin), End(End) {}

  ReflectionSection(uint64_t Begin, uint64_t End)
  : Begin(reinterpret_cast<const void *>(Begin)),
    End(reinterpret_cast<const void *>(End)) {}

  void *startAddress() {
    return const_cast<void *>(Begin);
  }

  const_iterator begin() const {
    return const_iterator(Begin, End);
  }

  const_iterator end() const {
    return const_iterator(End, End);
  }

  size_t size() const {
    return (char *)End - (char *)Begin;
  }
};

using FieldSection = ReflectionSection<FieldDescriptorIterator>;
using AssociatedTypeSection = ReflectionSection<AssociatedTypeIterator>;
using BuiltinTypeSection = ReflectionSection<BuiltinTypeDescriptorIterator>;
using GenericSection = ReflectionSection<const void *>;

struct ReflectionInfo {
  std::string ImageName;
  FieldSection fieldmd;
  AssociatedTypeSection assocty;
  BuiltinTypeSection builtin;
  GenericSection typeref;
  GenericSection reflstr;
};

/// An implementation of MetadataReader's BuilderType concept for
/// building TypeRefs, and parsing field metadata from any images
/// it has been made aware of.
///
/// Note that the TypeRefBuilder owns the memory for all TypeRefs
/// it vends.
class TypeRefBuilder {
public:
  using BuiltType = const TypeRef *;
  using BuiltNominalTypeDecl = Optional<std::string>;

  TypeRefBuilder();

  TypeRefBuilder(const TypeRefBuilder &other) = delete;
  TypeRefBuilder &operator=(const TypeRefBuilder &other) = delete;

private:
  std::vector<std::unique_ptr<const TypeRef>> TypeRefPool;
  TypeConverter TC;

public:
  template <typename TypeRefTy, typename... Args>
  TypeRefTy *makeTypeRef(Args... args) {
    auto TR = new TypeRefTy(::std::forward<Args>(args)...);
    TypeRefPool.push_back(std::unique_ptr<const TypeRef>(TR));
    return TR;
  }

  ///
  /// Factory methods for all TypeRef kinds
  ///

  const BuiltinTypeRef *createBuiltinType(const std::string &mangledName) {
    return BuiltinTypeRef::create(*this, mangledName);
  }

  Optional<std::string> createNominalTypeDecl(const Demangle::NodePointer &node) {
    return Demangle::mangleNode(node);
  }

  Optional<std::string> createNominalTypeDecl(std::string &&mangledName) {
    return std::move(mangledName);
  }

  const NominalTypeRef *createNominalType(
                                    const Optional<std::string> &mangledName,
                                    const TypeRef *parent) {
    return NominalTypeRef::create(*this, *mangledName, parent);
  }

  const BoundGenericTypeRef *
  createBoundGenericType(const Optional<std::string> &mangledName,
                         const std::vector<const TypeRef *> &args,
                         const TypeRef *parent) {
    return BoundGenericTypeRef::create(*this, *mangledName, args, parent);
  }

  const TupleTypeRef *
  createTupleType(const std::vector<const TypeRef *> &elements,
                  bool isVariadic) {
    return TupleTypeRef::create(*this, elements, isVariadic);
  }

  const FunctionTypeRef *
  createFunctionType(const std::vector<const TypeRef *> &args,
                     const std::vector<bool> &inOutArgs,
                     const TypeRef *result,
                     FunctionTypeFlags flags) {
    // FIXME: don't ignore inOutArgs
    return FunctionTypeRef::create(*this, args, result, flags);
  }

  const ProtocolTypeRef *createProtocolType(const std::string &moduleName,
                                      const std::string &protocolName) {
    return ProtocolTypeRef::create(*this, moduleName, protocolName);
  }

  const ProtocolCompositionTypeRef *
  createProtocolCompositionType(const std::vector<const TypeRef*> &protocols) {
    for (auto protocol : protocols) {
      if (!isa<ProtocolTypeRef>(protocol))
        return nullptr;
    }
    return ProtocolCompositionTypeRef::create(*this, protocols);
  }

  const ExistentialMetatypeTypeRef *
  createExistentialMetatypeType(const TypeRef *instance) {
    return ExistentialMetatypeTypeRef::create(*this, instance);
  }

  const MetatypeTypeRef *createMetatypeType(const TypeRef *instance) {
    return MetatypeTypeRef::create(*this, instance);
  }

  const GenericTypeParameterTypeRef *
  createGenericTypeParameterType(unsigned depth, unsigned index) {
    return GenericTypeParameterTypeRef::create(*this, depth, index);
  }

  const DependentMemberTypeRef *
  createDependentMemberType(const std::string &member,
                            const TypeRef *base,
                            const TypeRef *protocol) {
    if (!isa<ProtocolTypeRef>(protocol))
      return nullptr;
    return DependentMemberTypeRef::create(*this, member, base, protocol);
  }

  const UnownedStorageTypeRef *createUnownedStorageType(const TypeRef *base) {
    return UnownedStorageTypeRef::create(*this, base);
  }

  const UnmanagedStorageTypeRef *
  createUnmanagedStorageType(const TypeRef *base) {
    return UnmanagedStorageTypeRef::create(*this, base);
  }

  const WeakStorageTypeRef *createWeakStorageType(const TypeRef *base) {
    return WeakStorageTypeRef::create(*this, base);
  }

  const ObjCClassTypeRef *getUnnamedObjCClassType() {
    return ObjCClassTypeRef::getUnnamed();
  }

  const ForeignClassTypeRef *getUnnamedForeignClassType() {
    return ForeignClassTypeRef::getUnnamed();
  }

  const OpaqueTypeRef *getOpaqueType() {
    return OpaqueTypeRef::get();
  }

  ///
  /// Parsing reflection metadata
  ///

  void addReflectionInfo(ReflectionInfo I) {
    ReflectionInfos.push_back(I);
  }

private:

  std::vector<ReflectionInfo> ReflectionInfos;

  const AssociatedTypeDescriptor *
  lookupAssociatedTypes(const std::string &MangledTypeName,
                        const DependentMemberTypeRef *DependentMember);

public:
  TypeConverter &getTypeConverter() { return TC; }

  const TypeRef *
  getDependentMemberTypeRef(const std::string &MangledTypeName,
                            const DependentMemberTypeRef *DependentMember);

  /// Load unsubstituted field types for a nominal type.
  const FieldDescriptor *getFieldTypeInfo(const TypeRef *TR);

  /// Get the parsed and substituted field types for a nominal type.
  std::vector<std::pair<std::string, const TypeRef *>>
  getFieldTypeRefs(const TypeRef *TR, const FieldDescriptor *FD);

  /// Get the primitive type lowering for a builtin type.
  const BuiltinTypeDescriptor *getBuiltinTypeInfo(const TypeRef *TR);

  ///
  /// Dumping typerefs, field declarations, associated types
  ///

  void dumpTypeRef(const std::string &MangledName,
                   std::ostream &OS, bool printTypeName = false);
  void dumpFieldSection(std::ostream &OS);
  void dumpAssociatedTypeSection(std::ostream &OS);
  void dumpBuiltinTypeSection(std::ostream &OS);
  void dumpAllSections(std::ostream &OS);
};


} // end namespace reflection
} // end namespace swift

#endif // SWIFT_REFLECTION_TYPEREFBUILDER_H
