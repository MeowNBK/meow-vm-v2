#pragma once

#include "common/pch.h"
#include "core/objects/module.h"
#include "core/type.h"

namespace meow { class MeowEngine; }
namespace meow { class MemoryManager; }

namespace meow {
class ModuleManager {
   public:
    explicit ModuleManager(meow::MemoryManager* heap, meow::MeowEngine* engine) noexcept;
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager(ModuleManager&&) = default;
    ModuleManager& operator=(const ModuleManager&) = delete;
    ModuleManager& operator=(ModuleManager&&) = default;
    ModuleManager() = default;
    meow::module_t load_module(meow::string_t module_path, meow::string_t importer_path);

    inline void reset_cache() noexcept {
        module_cache_.clear();
    }

    inline void add_cache(meow::string_t name, const meow::module_t& mod) {
        module_cache_[name] = mod;
    }

   private:
    std::unordered_map<meow::string_t, meow::module_t> module_cache_;
    meow::string_t entry_path_;

    meow::MemoryManager* heap_;
    meow::MeowEngine* engine_;
};
}  // namespace meow::module