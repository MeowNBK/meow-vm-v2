#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "core/op_codes.h"
#include "core/value.h"
#include "core/meow_object.h"

namespace meow { class MemoryManager; }

namespace meow {

constexpr size_t NUM_VALUE_TYPES = static_cast<size_t>(meow::ValueType::TotalValueTypes);
constexpr size_t NUM_OPCODES = static_cast<size_t>(meow::OpCode::TOTAL_OPCODES);
using binary_function_t = meow::return_t (*)(meow::param_t, meow::param_t);
using unary_function_t = meow::return_t (*)(meow::param_t);

[[nodiscard]] inline constexpr size_t operator+(meow::ValueType value_type) noexcept {
    return static_cast<size_t>(value_type);
}
[[nodiscard]] inline constexpr size_t operator+(meow::OpCode op_code) noexcept {
    return static_cast<size_t>(op_code);
}

inline meow::ValueType get_value_type(meow::param_t value) noexcept {
    using namespace meow;
    ValueType type = static_cast<ValueType>(value.index());
    if (type == ValueType::Object) {
        return static_cast<ValueType>(value.as_object()->get_type());
    }
    return type;
}

class OperatorDispatcher {
public:
    explicit OperatorDispatcher(meow::MemoryManager* heap) noexcept;

    [[nodiscard]] inline binary_function_t find(meow::OpCode op_code, meow::param_t left, meow::param_t right) const noexcept {
        auto left_type = get_value_type(left);
        auto right_type = get_value_type(right);
        return binary_dispatch_table_[+op_code][+left_type][+right_type];
    }

    [[nodiscard]] inline unary_function_t find(meow::OpCode op_code, meow::param_t right) const noexcept {
        auto right_type = get_value_type(right);
        return unary_dispatch_table_[+op_code][+right_type];
    }

    // [[nodiscard]] inline binary_function_t operator[](core::OpCode op, core::ValueType lhs, core::ValueType rhs) const noexcept {
    //     return binary_dispatch_table_[+op][+lhs][+rhs];
    // }
private:
    [[maybe_unused]] meow::MemoryManager* heap_;
    binary_function_t binary_dispatch_table_[NUM_OPCODES][NUM_VALUE_TYPES][NUM_VALUE_TYPES];
    unary_function_t unary_dispatch_table_[NUM_OPCODES][NUM_VALUE_TYPES];
};
}  // namespace meow::runtime