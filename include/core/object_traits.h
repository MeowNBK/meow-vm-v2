// include/core/object_traits.h

#pragma once

#include "core/meow_object.h"
#include "core/type.h"

namespace meow {

namespace detail {

template <typename T>
struct object_traits {
    static_assert(std::is_base_of_v<MeowObject, T>, 
                  "as_if<T> only works on types derived from MeowObject");
};

template <> struct object_traits<meow::ObjArray> {
    static constexpr ObjectType type_tag = ObjectType::ARRAY;
};
template <> struct object_traits<meow::ObjString> {
    static constexpr ObjectType type_tag = ObjectType::STRING;
};
template <> struct object_traits<meow::ObjHashTable> {
    static constexpr ObjectType type_tag = ObjectType::HASH_TABLE;
};
template <> struct object_traits<meow::ObjInstance> {
    static constexpr ObjectType type_tag = ObjectType::INSTANCE;
};
template <> struct object_traits<meow::ObjClass> {
    static constexpr ObjectType type_tag = ObjectType::CLASS;
};
template <> struct object_traits<meow::ObjBoundMethod> {
    static constexpr ObjectType type_tag = ObjectType::BOUND_METHOD;
};
template <> struct object_traits<meow::ObjUpvalue> {
    static constexpr ObjectType type_tag = ObjectType::UPVALUE;
};
template <> struct object_traits<meow::ObjFunctionProto> {
    static constexpr ObjectType type_tag = ObjectType::PROTO;
};
template <> struct object_traits<meow::ObjClosure> {
    static constexpr ObjectType type_tag = ObjectType::FUNCTION;
};
template <> struct object_traits<meow::ObjModule> {
    static constexpr ObjectType type_tag = ObjectType::MODULE;
};

} // namespace detail
} // namespace meow::core