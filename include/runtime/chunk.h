#pragma once

#include "common/pch.h"
#include "common/definitions.h"
#include "core/value.h"

namespace meow { class TextParser; }

namespace meow {
class Chunk {
   public:
    Chunk() = default;
    Chunk(std::vector<uint8_t>&& code, std::vector<meow::Value>&& constants) noexcept : code_(std::move(code)), constant_pool_(std::move(constants)) {
    }

    // --- Modifiers ---
    inline void write_byte(uint8_t byte) {
        code_.push_back(byte);
    }

    inline void write_u16(uint16_t value) {
        code_.push_back(static_cast<uint8_t>(value & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    inline void write_u32(uint32_t value) {
        code_.push_back(static_cast<uint8_t>(value & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    inline void write_u64(uint64_t value) {
        code_.push_back(static_cast<uint8_t>(value & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
        code_.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
    }

    inline void write_f64(double value) {
        write_u64(std::bit_cast<uint64_t>(value));
    }
    // --- Code buffer ---
    [[nodiscard]] inline const uint8_t* get_code() const noexcept {
        return code_.data();
    }
    [[nodiscard]] inline size_t get_code_size() const noexcept {
        return code_.size();
    }
    [[nodiscard]] inline bool is_code_empty() const noexcept {
        return code_.empty();
    }

    // --- Constant pool ---
    [[nodiscard]] inline size_t get_pool_size() const noexcept {
        return constant_pool_.size();
    }
    [[nodiscard]] inline bool is_pool_empty() const noexcept {
        return constant_pool_.empty();
    }
    [[nodiscard]] inline size_t add_constant(meow::param_t value) {
        constant_pool_.push_back(value);
        return constant_pool_.size() - 1;
    }
    [[nodiscard]] inline meow::return_t get_constant(size_t index) const noexcept {
        return constant_pool_[index];
    }
    [[nodiscard]] inline meow::value_t& get_constant_ref(size_t index) noexcept {
        return constant_pool_[index];
    }
    [[nodiscard]] inline const uint8_t* get_code_buffer_ptr() const noexcept {
        return code_.data();
    }

    inline bool patch_u16(size_t offset, uint16_t value) noexcept {
        if (offset + 1 >= code_.size()) return false;

        code_[offset] = static_cast<uint8_t>(value & 0xFF);
        code_[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);

        return true;
    }

   private:
    std::vector<uint8_t> code_;
    std::vector<meow::Value> constant_pool_;
};
}  // namespace meow::runtime