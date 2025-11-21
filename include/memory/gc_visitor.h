#pragma once

#include "core/definitions.h"

namespace meow::inline core {
class Value;
struct MeowObject;
}  // namespace meow::core

namespace meow::inline memory {
struct GCVisitor {
    virtual ~GCVisitor() = default;
    virtual void visit_value(meow::param_t value) noexcept = 0;
    virtual void visit_object(const meow::MeowObject* object) noexcept = 0;
};
}  // namespace meow::memory