#pragma once

#include "common/pch.h"
#include "utils/types/variant.h"

namespace meow::inline vm { class Machine; }

namespace meow::inline core::inline objects {
    class ObjString;
    class ObjArray;
    class ObjHashTable;
    class ObjClass;
    class ObjInstance;
    class ObjBoundMethod;
    class ObjUpvalue;
    class ObjFunctionProto;
    class ObjNativeFunction;
    class ObjClosure;
    class ObjModule;
}  // namespace meow::objects

namespace meow::inline core {
    struct MeowObject;
    
    using null_t = std::monostate;
    using bool_t = bool;
    using int_t = int64_t;
    using float_t = double;
    using native_fn_t = value_t (*)(meow::Machine* engine, int argc, value_t* argv);
    
    using object_t = MeowObject*;
    
    using array_t = meow::ObjArray*;
    using string_t = meow::ObjString*;
    using hash_table_t = meow::ObjHashTable*;
    using instance_t = meow::ObjInstance*;
    using class_t = meow::ObjClass*;
    using bound_method_t = meow::ObjBoundMethod*;
    using upvalue_t = meow::ObjUpvalue*;
    using proto_t = meow::ObjFunctionProto*;
    using function_t = meow::ObjClosure*;
    // using native_fn_t = meow::ObjNativeFunction*;
    using module_t = meow::ObjModule*;

    enum class ValueType : uint8_t {
        Null,
        Bool,
        Int,
        Float,
        Object,

        Array,        // 1  — ARRAY
        String,       // 2  — STRING
        HashTable,    // 3  — HASH_TABLE
        Instance,     // 4  — INSTANCE
        Class,        // 5  — CLASS
        BoundMethod,  // 6  — BOUND_METHOD
        Upvalue,      // 7  — UPVALUE
        Proto,        // 8  — PROTO
        Function,     // 9  — FUNCTION
        // NativeFn,     // 10 — NATIVE_FN
        Module,       // 11 — MODULE

        TotalValueTypes
    };
}  // namespace meow::core