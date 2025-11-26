#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "core/op_codes.h"
#include "core/value.h"
#include "core/meow_object.h"


namespace meow {
class MemoryManager;
constexpr size_t NUM_VALUE_TYPES = static_cast<size_t>(ValueType::TotalValueTypes);
constexpr size_t NUM_OPCODES = static_cast<size_t>(OpCode::TOTAL_OPCODES);
using binary_function_t = return_t (*)(param_t, param_t);
using unary_function_t = return_t (*)(param_t);

inline constexpr size_t operator+(ValueType value_type) noexcept {
    return static_cast<size_t>(std::to_underlying(value_type));
}
inline constexpr size_t operator+(OpCode op_code) noexcept {
    return static_cast<size_t>(std::to_underlying(op_code));
}

inline ValueType get_value_type(param_t value) noexcept {
    ValueType type = static_cast<ValueType>(value.index());
    if (type == ValueType::Object) {
        return static_cast<ValueType>(value.as_object()->get_type());
    }
    return type;
}

class OperatorDispatcher {
public:
    explicit OperatorDispatcher(MemoryManager* heap) noexcept;

    inline binary_function_t find(OpCode op_code, param_t left, param_t right) const noexcept {
        auto left_type = get_value_type(left);
        auto right_type = get_value_type(right);
        return binary_dispatch_table_[+op_code][+left_type][+right_type];
    }

    inline unary_function_t find(OpCode op_code, param_t right) const noexcept {
        auto right_type = get_value_type(right);
        return unary_dispatch_table_[+op_code][+right_type];
    }

    inline binary_function_t operator[](OpCode op, ValueType lhs, ValueType rhs) const noexcept {
        return binary_dispatch_table_[+op][+lhs][+rhs];
    }

    inline unary_function_t operator[](OpCode op, ValueType rhs) const noexcept {
        return unary_dispatch_table_[+op][+rhs];
    }
private:
    [[maybe_unused]] MemoryManager* heap_;
    binary_function_t binary_dispatch_table_[NUM_OPCODES][NUM_VALUE_TYPES][NUM_VALUE_TYPES];
    unary_function_t unary_dispatch_table_[NUM_OPCODES][NUM_VALUE_TYPES];
};
}