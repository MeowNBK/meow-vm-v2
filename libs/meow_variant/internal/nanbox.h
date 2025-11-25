#pragma once

#include <bit>          // std::bit_cast
#include <cstdint>      // uint64_t
#include <utility>      // std::unreachable (C++23)
#include <variant>      // std::monostate
#include <stdexcept>    // std::bad_variant_access
#include "utils.h"      // variant_utils

namespace meow::utils {

// Check Platform (Giữ nguyên)
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__))
    #define MEOW_LITTLE_64 1
#else
    #define MEOW_LITTLE_64 0
#endif

// Classifiers (Giữ nguyên)
template <typename T> concept PointerLike  = std::is_pointer_v<std::decay_t<T>>;
template <typename T> concept IntegralLike = std::is_integral_v<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, bool>;
template <typename T> concept DoubleLike   = std::is_floating_point_v<std::decay_t<T>>;
template <typename T> concept BoolLike     = std::is_same_v<std::decay_t<T>, bool>;

// Nanbox check (Giữ nguyên)
template <typename List> struct all_nanboxable_impl;
template <typename... Ts>
struct all_nanboxable_impl<detail::type_list<Ts...>> {
    static constexpr bool value = ((sizeof(std::decay_t<Ts>) <= 8 && 
        (DoubleLike<Ts> || IntegralLike<Ts> || PointerLike<Ts> || 
         std::is_same_v<std::decay_t<Ts>, std::monostate> || BoolLike<Ts>)) && ...);
};

// Constants (Giữ nguyên)
static constexpr uint64_t MEOW_EXP_MASK     = 0x7FF0000000000000ULL;
static constexpr uint64_t MEOW_QNAN_PREFIX  = 0x7FF8000000000000ULL;
static constexpr uint64_t MEOW_TAG_MASK     = 0x0007000000000000ULL;
static constexpr uint64_t MEOW_PAYLOAD_MASK = 0x0000FFFFFFFFFFFFULL;
static constexpr unsigned MEOW_TAG_SHIFT    = 48;
static constexpr uint64_t MEOW_VALUELESS    = 0xFFFFFFFFFFFFFFFFULL;

// Inline Helper
[[nodiscard]] __attribute__((always_inline)) 
inline bool is_double(uint64_t b) { return (b & MEOW_EXP_MASK) != MEOW_EXP_MASK; }

// ============================================================================
// NaNBoxedVariant (Final Optimized)
// ============================================================================
template <typename... Args>
class NaNBoxedVariant {
    using flat_list = meow::utils::flattened_unique_t<Args...>;
    static constexpr std::size_t count = detail::type_list_length<flat_list>::value;
    static constexpr std::size_t dbl_idx = detail::type_list_index_of<double, flat_list>::value;

    // Compile-time check: NaN-boxing chỉ hỗ trợ tối đa 8 loại type (3-bit tag)
    static_assert(count <= 8, "NaNBoxedVariant supports max 8 alternative types.");

public:
    using inner_types = flat_list;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    // --- Constructors ---
    NaNBoxedVariant() noexcept {
        if constexpr (std::is_same_v<typename detail::nth_type<0, flat_list>::type, std::monostate>) {
            bits_ = MEOW_QNAN_PREFIX; // Default to Monostate (Tag 0)
        } else {
            bits_ = MEOW_VALUELESS;
        }
    }

    NaNBoxedVariant(const NaNBoxedVariant&) = default;
    NaNBoxedVariant(NaNBoxedVariant&& o) noexcept : bits_(o.bits_) { o.bits_ = MEOW_VALUELESS; }

    template <typename T>
    requires (detail::type_list_index_of<std::decay_t<T>, flat_list>::value != detail::invalid_index)
    NaNBoxedVariant(T&& v) noexcept {
        using U = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        bits_ = encode<U>(idx, std::forward<T>(v));
    }

    // --- Assignment ---
    NaNBoxedVariant& operator=(const NaNBoxedVariant&) = default;
    NaNBoxedVariant& operator=(NaNBoxedVariant&& o) noexcept {
        bits_ = o.bits_; o.bits_ = MEOW_VALUELESS; return *this;
    }

    template <typename T>
    requires (detail::type_list_index_of<std::decay_t<T>, flat_list>::value != detail::invalid_index)
    NaNBoxedVariant& operator=(T&& v) noexcept {
        using U = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        bits_ = encode<U>(idx, std::forward<T>(v));
        return *this;
    }

    // --- Observers ---
    [[nodiscard]] std::size_t index() const noexcept {
        if (bits_ == MEOW_VALUELESS) return npos;
        if (is_double(bits_)) return dbl_idx;
        return static_cast<std::size_t>((bits_ >> MEOW_TAG_SHIFT) & 0x7);
    }

    [[nodiscard]] bool valueless() const noexcept { return bits_ == MEOW_VALUELESS; }

    template <typename T>
    [[nodiscard]] bool holds() const noexcept {
        // Optim: Nếu check double, chỉ cần check bit mask, không cần check tag
        using U = std::decay_t<T>;
        if constexpr (DoubleLike<U>) {
            return is_double(bits_) && bits_ != MEOW_VALUELESS;
        } else {
            constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
            // Check nhanh: không phải double VÀ tag khớp
            return !is_double(bits_) && 
                   ((bits_ >> MEOW_TAG_SHIFT) & 0x7) == idx && 
                   bits_ != MEOW_VALUELESS;
        }
    }

    // --- Accessors ---
    
    // Unsafe get (Fastest)
    template <typename T>
    [[nodiscard]] std::decay_t<T> get() const noexcept {
        return decode<std::decay_t<T>>(bits_);
    }

    // Safe get (Throws)
    template <typename T>
    [[nodiscard]] std::decay_t<T> safe_get() const {
        if (!holds<T>()) [[unlikely]] throw std::bad_variant_access();
        return decode<std::decay_t<T>>(bits_);
    }

    // Pointer get (API Compatible)
    template <typename T>
    [[nodiscard]] std::decay_t<T>* get_if() noexcept {
        if (!holds<T>()) return nullptr;
        thread_local static std::decay_t<T> temp;
        temp = decode<std::decay_t<T>>(bits_);
        return &temp;
    }
    
    template <typename T>
    [[nodiscard]] const std::decay_t<T>* get_if() const noexcept {
        return const_cast<NaNBoxedVariant*>(this)->get_if<T>();
    }

    void swap(NaNBoxedVariant& o) noexcept { std::swap(bits_, o.bits_); }
    bool operator==(const NaNBoxedVariant& o) const { return bits_ == o.bits_; }
    bool operator!=(const NaNBoxedVariant& o) const { return bits_ != o.bits_; }

    // --- C++23 Optimized Visit (Switch-Based) ---
    template <typename Self, typename Visitor>
    decltype(auto) visit(this Self&& self, Visitor&& vis) {
        // Lấy index
        const std::size_t idx = self.index();
        
        // Optimize: double thường là case phổ biến nhất trong VM số học, check trước
        if (idx == dbl_idx) [[likely]] {
             return std::invoke(std::forward<Visitor>(vis), 
                 self.template decode<double>(self.bits_));
        }

        if (idx == npos) [[unlikely]] throw std::bad_variant_access();

        // Magic: Tạo switch-case tại thời điểm biên dịch
        return self.visit_switch(std::forward<Visitor>(vis), idx, std::make_index_sequence<count>{});
    }

private:
    uint64_t bits_;

    // --- Encoder ---
    template <typename T>
    static __attribute__((always_inline)) uint64_t encode(std::size_t idx, T v) noexcept {
        using U = std::decay_t<T>;
        if constexpr (DoubleLike<U>) {
            uint64_t b = std::bit_cast<uint64_t>(static_cast<double>(v));
            // Canonicalize: Nếu trùng QNaN prefix, đổi bit thấp nhất để tránh nhầm lẫn
            if (!is_double(b)) return b; 
            if (b == MEOW_VALUELESS) [[unlikely]] return b ^ 1; 
            return b;
        } else {
            uint64_t payload = 0;
            if constexpr (PointerLike<U>) {
                payload = reinterpret_cast<uintptr_t>(static_cast<const void*>(v));
            } else if constexpr (IntegralLike<U>) {
                // Cast về uint64_t
                payload = static_cast<uint64_t>(static_cast<int64_t>(v));
            } else if constexpr (BoolLike<U>) {
                // Branchless set boolean
                payload = static_cast<uint64_t>(!!v);
            }
            // Tagging
            return MEOW_QNAN_PREFIX | (static_cast<uint64_t>(idx) << MEOW_TAG_SHIFT) | (payload & MEOW_PAYLOAD_MASK);
        }
    }

    // --- Decoder ---
    template <typename T>
    static __attribute__((always_inline)) T decode(uint64_t bits) noexcept {
        using U = std::decay_t<T>;
        if constexpr (DoubleLike<U>) {
            return std::bit_cast<double>(bits);
        } else if constexpr (IntegralLike<U>) {
            // Sign-extension trick (nhanh nhất để khôi phục số âm 48-bit)
            return static_cast<U>(static_cast<int64_t>(bits << 16) >> 16);
        } else if constexpr (PointerLike<U>) {
            return reinterpret_cast<U>(static_cast<uintptr_t>(bits & MEOW_PAYLOAD_MASK));
        } else if constexpr (BoolLike<U>) {
            return static_cast<U>((bits & MEOW_PAYLOAD_MASK));
        } else {
            return U{};
        }
    }

    // --- Visit Switch Generator ---
    template <typename Visitor, std::size_t... Is>
    decltype(auto) visit_switch(Visitor&& vis, std::size_t idx, std::index_sequence<Is...>) const {
        // C++23: std::unreachable() giúp optimizer loại bỏ bound check thừa
        auto dispatch = [&](auto I_CONST) -> decltype(auto) {
            constexpr std::size_t I = decltype(I_CONST)::value;
            using T = typename detail::nth_type<I, flat_list>::type;
            return std::invoke(std::forward<Visitor>(vis), decode<T>(bits_));
        };

        // Switch case thủ công (nhanh hơn jump table vì cho phép inlining)
        // Vì count <= 8, switch là tối ưu nhất.
        switch (idx) {
            case 0: if constexpr (0 < count) return dispatch(std::integral_constant<std::size_t, 0>{}); [[fallthrough]];
            case 1: if constexpr (1 < count) return dispatch(std::integral_constant<std::size_t, 1>{}); [[fallthrough]];
            case 2: if constexpr (2 < count) return dispatch(std::integral_constant<std::size_t, 2>{}); [[fallthrough]];
            case 3: if constexpr (3 < count) return dispatch(std::integral_constant<std::size_t, 3>{}); [[fallthrough]];
            case 4: if constexpr (4 < count) return dispatch(std::integral_constant<std::size_t, 4>{}); [[fallthrough]];
            case 5: if constexpr (5 < count) return dispatch(std::integral_constant<std::size_t, 5>{}); [[fallthrough]];
            case 6: if constexpr (6 < count) return dispatch(std::integral_constant<std::size_t, 6>{}); [[fallthrough]];
            case 7: if constexpr (7 < count) return dispatch(std::integral_constant<std::size_t, 7>{}); [[fallthrough]];
            default: std::unreachable(); // C++23: Trust me, this never happens
        }
    }
};

template <typename... Ts> void swap(NaNBoxedVariant<Ts...>& a, NaNBoxedVariant<Ts...>& b) noexcept { a.swap(b); }

} // namespace