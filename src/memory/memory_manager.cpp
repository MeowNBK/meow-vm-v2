#include "memory/memory_manager.h"
#include "core/objects.h"

using namespace meow;
using namespace meow;

namespace meow {

MemoryManager::MemoryManager(std::unique_ptr<GarbageCollector> gc) noexcept : gc_(std::move(gc)), gc_threshold_(1024), object_allocated_(0) {
}

MemoryManager::~MemoryManager() noexcept = default;

string_t MemoryManager::new_string(std::string_view str_view) noexcept {
    auto it = string_pool_.find(str_view);
    if (it != string_pool_.end()) {
        return it->second;
    }
    
    std::string s(str_view);
    string_t new_obj = new_object<meow::ObjString>(s);
    string_pool_.emplace(std::move(s), new_obj);
    return new_obj;
}

string_t MemoryManager::new_string(const char* chars, size_t length) noexcept {
    return new_string(std::string(chars, length));
}

array_t MemoryManager::new_array(const std::vector<Value>& elements) noexcept {
    return new_object<meow::ObjArray>(elements);
}

hash_table_t MemoryManager::new_hash(const std::unordered_map<string_t, Value>& fields) noexcept {
    return new_object<meow::ObjHashTable>(fields);
}

upvalue_t MemoryManager::new_upvalue(size_t index) noexcept {
    return new_object<meow::ObjUpvalue>(index);
}

proto_t MemoryManager::new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk) noexcept {
    return new_object<meow::ObjFunctionProto>(registers, upvalues, name, std::move(chunk));
}

proto_t MemoryManager::new_proto(size_t registers, size_t upvalues, string_t name, Chunk&& chunk, std::vector<meow::UpvalueDesc>&& descs) noexcept {
    return new_object<meow::ObjFunctionProto>(registers, upvalues, name, std::move(chunk), std::move(descs));
}

function_t MemoryManager::new_function(proto_t proto) noexcept {
    return new_object<meow::ObjClosure>(proto);
}

module_t MemoryManager::new_module(string_t file_name, string_t file_path, proto_t main_proto) noexcept {
    return new_object<meow::ObjModule>(file_name, file_path, main_proto);
}

class_t MemoryManager::new_class(string_t name) noexcept {
    return new_object<meow::ObjClass>(name);
}

instance_t MemoryManager::new_instance(class_t klass) noexcept {
    return new_object<meow::ObjInstance>(klass);
}

bound_method_t MemoryManager::new_bound_method(instance_t instance, function_t function) noexcept {
    return new_object<meow::ObjBoundMethod>(instance, function);
}

};  // namespace meow::memory