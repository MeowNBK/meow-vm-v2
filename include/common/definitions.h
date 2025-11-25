#pragma once

#include "common/pch.h"

namespace meow { struct MeowObject; }
namespace meow { class Machine; }
namespace meow {
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
}


namespace meow {

class Value;

using value_t = Value;
using param_t = value_t;
using return_t = value_t;
using mutable_t = value_t&;
using arguments_t = std::vector<value_t>&;
    
using null_t = std::monostate;
using bool_t = bool;
using int_t = int64_t;
using float_t = double;
using native_t = value_t (*)(meow::Machine* engine, int argc, value_t* argv);
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
// using native_t = meow::ObjNativeFunction*;
using module_t = meow::ObjModule*;
}