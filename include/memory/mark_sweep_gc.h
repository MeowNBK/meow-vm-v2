#pragma once

#include "common/pch.h"
#include "core/definitions.h"
#include "memory/garbage_collector.h"
#include "memory/gc_visitor.h"

namespace meow::inline runtime {
struct ExecutionContext;
struct BuiltinRegistry;
}  // namespace meow::runtime

namespace meow::inline memory {
struct GCMetadata {
    bool is_marked_ = false;
};

class MarkSweepGC : public GarbageCollector, public GCVisitor {
public:
    explicit MarkSweepGC(meow::ExecutionContext* context, meow::BuiltinRegistry* builtins) noexcept : context_(context), builtins_(builtins) {
    }
    ~MarkSweepGC() noexcept override;

    // -- Collector ---
    void register_object(const meow::MeowObject* object) override;
    size_t collect() noexcept override;

    // --- Visitor ---
    void visit_value(meow::param_t value) noexcept override;
    void visit_object(const meow::MeowObject* object) noexcept override;
private:
    std::unordered_map<const meow::MeowObject*, GCMetadata> metadata_;
    meow::ExecutionContext* context_ = nullptr;
    meow::BuiltinRegistry* builtins_ = nullptr;

    void mark(const meow::MeowObject* object);

};
}  // namespace meow::memory