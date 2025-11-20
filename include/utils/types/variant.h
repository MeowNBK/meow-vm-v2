#pragma once

#include <array>        // std::array
#include <bit>          // std::bit_cast
#include <cmath>        // std::isnan
#include <cstddef>      // std::size_t, static_cast
#include <cstdint>      // uint64_t, uint8_t
#include <limits>       // std::numeric_limits
#include <stdexcept>    // std::bad_variant_access
#include <type_traits>  // std::is_same, std::decay_t
#include <utility>      // std::forward, std::move
#include <variant>      // std::monostate

#include "variant_utils.h" // Giả định file này nằm cùng thư mục hoặc đường dẫn include đúng

namespace meow::utils {

// ---------------------- Platform / Macros ----------------------
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#define MEOW_BYTE_ORDER __BYTE_ORDER__
#define MEOW_ORDER_LITTLE __ORDER_LITTLE_ENDIAN__
#endif

#if (defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)) && (defined(MEOW_BYTE_ORDER) && MEOW_BYTE_ORDER == MEOW_ORDER_LITTLE)
#define MEOW_LITTLE_64 1
#else
#define MEOW_LITTLE_64 0
#endif

static_assert(MEOW_LITTLE_64, "variant_nanbox.h requires 64-bit little-endian platform.");

#if defined(__GNUC__) || defined(__clang__)
#define MEOW_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define MEOW_ALWAYS_INLINE inline
#endif

#if defined(__GNUC__) || defined(__clang__)
#define MEOW_LIKELY(x) (__builtin_expect(!!(x), 1))
#define MEOW_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define MEOW_LIKELY(x) (x)
#define MEOW_UNLIKELY(x) (x)
#endif

// ---------------------- Classifiers ----------------------
template <typename T>
struct is_pointer_like {
    static constexpr bool value = std::is_pointer<std::decay_t<T>>::value && sizeof(std::decay_t<T>) <= 8 && alignof(std::decay_t<T>) <= 8;
};
template <typename T>
struct is_integral_like {
    static constexpr bool value = std::is_integral<std::decay_t<T>>::value && sizeof(std::decay_t<T>) <= 8 && !std::is_same<std::decay_t<T>, bool>::value;
};
template <typename T>
struct is_double_like {
    static constexpr bool value = std::is_floating_point<std::decay_t<T>>::value && sizeof(std::decay_t<T>) == 8;
};
template <typename T>
struct is_monostate_like {
    static constexpr bool value = std::is_same<std::decay_t<T>, std::monostate>::value;
};
template <typename T>
struct is_bool_like {
    static constexpr bool value = std::is_same<std::decay_t<T>, bool>::value;
};

// ---------------------- Check Nanboxability ----------------------
template <typename List>
struct all_nanboxable_impl;

template <>
struct all_nanboxable_impl<detail::type_list<>> : std::true_type {};

template <typename H, typename... Ts>
struct all_nanboxable_impl<detail::type_list<H, Ts...>>
    : std::integral_constant<bool, (is_double_like<H>::value || is_integral_like<H>::value || is_pointer_like<H>::value || is_monostate_like<H>::value || is_bool_like<H>::value) &&
                                       all_nanboxable_impl<detail::type_list<Ts...>>::value> {};

// ---------------------- NaN-box Constants ----------------------
// IEEE 754 Double: [Sign:1] [Exp:11] [Mantissa:52]
// NaN: Exp = 0x7FF.
// QNaN: Bit 51 = 1.
static constexpr uint64_t MEOW_EXP_MASK = 0x7FF0000000000000ULL;
static constexpr uint64_t MEOW_QNAN_BIT = 0x0008000000000000ULL; // Bit 51
static constexpr uint64_t MEOW_QNAN_PREFIX = (MEOW_EXP_MASK | MEOW_QNAN_BIT); // 0x7FF8...

// Tagging: We use bits 48, 49, 50.
static constexpr unsigned MEOW_TAG_SHIFT = 48;
static constexpr uint64_t MEOW_TAG_MASK = (0x7ULL << MEOW_TAG_SHIFT); // 3 bits tag
static constexpr uint64_t MEOW_PAYLOAD_MASK = ((1ULL << MEOW_TAG_SHIFT) - 1ULL); // 48 bits payload

// Sentinel for valueless state: A specific -NaN (Sign=1) with full payload.
// Valid encoded types usually have Sign=0 (from MEOW_QNAN_PREFIX).
static constexpr uint64_t MEOW_VALUELESS_BITS = 0xFFFFFFFFFFFFFFFFULL;

// ---------------------- Primitives ----------------------
MEOW_ALWAYS_INLINE uint64_t meow_bitcast_double_to_u64(double d) noexcept {
    return std::bit_cast<uint64_t>(d);
}
MEOW_ALWAYS_INLINE double meow_bitcast_u64_to_double(uint64_t u) noexcept {
    return std::bit_cast<double>(u);
}

// Checks if bits represent a "raw" double (i.e., NOT a NaN).
MEOW_ALWAYS_INLINE bool meow_is_raw_double(uint64_t b) noexcept {
    // A value is a raw double if its exponent is NOT all 1s.
    return ((b & MEOW_EXP_MASK) != MEOW_EXP_MASK);
}

MEOW_ALWAYS_INLINE uint64_t meow_payload_from_ptr(const void* p) noexcept {
    uintptr_t u = reinterpret_cast<uintptr_t>(p);
    return static_cast<uint64_t>(u & MEOW_PAYLOAD_MASK);
}

MEOW_ALWAYS_INLINE void* meow_ptr_from_payload(uint64_t payload) noexcept {
    uintptr_t u = static_cast<uintptr_t>(payload);
    return reinterpret_cast<void*>(u);
}

// ---------------------- NaNBoxedVariant (8 bytes) ----------------------
template <typename... Args>
class NaNBoxedVariant {
    using flat_list = detail::flattened_unique_t<Args...>;
    static constexpr std::size_t alternatives_count = detail::type_list_length<flat_list>::value;

    // We support max 8 alternatives because we use a 3-bit tag (0..7).
    static_assert(alternatives_count > 0, "Variant must have at least one alternative");
    static_assert(alternatives_count <= 8, "NaNBoxedVariant supports up to 8 alternatives (limited by 3-bit tag)");
    static_assert(all_nanboxable_impl<flat_list>::value, "All alternatives must be nanboxable (double, int<=64, ptr, bool, monostate)");

    static constexpr std::size_t dbl_idx = detail::type_list_index_of<double, flat_list>::value;
    static constexpr bool has_double = (dbl_idx != detail::invalid_index);

   public:
    using inner_types = flat_list;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    // --- Constructors ---
    MEOW_ALWAYS_INLINE NaNBoxedVariant() noexcept {
        if constexpr (std::is_same_v<typename detail::nth_type<0, flat_list>::type, std::monostate>) {
            // Construct index 0 (monostate) -> Encoded as Tag 0 | payload 0
            bits_ = MEOW_QNAN_PREFIX | (static_cast<uint64_t>(0) << MEOW_TAG_SHIFT);
        } else {
            // Default to valueless
            bits_ = MEOW_VALUELESS_BITS;
        }
    }

    MEOW_ALWAYS_INLINE NaNBoxedVariant(const NaNBoxedVariant& o) noexcept : bits_(o.bits_) {}
    
    MEOW_ALWAYS_INLINE NaNBoxedVariant(NaNBoxedVariant&& o) noexcept : bits_(o.bits_) {
        o.bits_ = MEOW_VALUELESS_BITS;
    }

    // Value constructor
    template <typename T, typename U = std::decay_t<T>, 
              typename = std::enable_if_t<(detail::type_list_index_of<U, flat_list>::value != detail::invalid_index)>>
    MEOW_ALWAYS_INLINE NaNBoxedVariant(T&& v) noexcept {
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        bits_ = encode_type<U>(idx, std::forward<T>(v));
    }

    // In-place type
    template <typename T, typename... CArgs, typename U = std::decay_t<T>, 
              typename = std::enable_if_t<(detail::type_list_index_of<U, flat_list>::value != detail::invalid_index)>>
    MEOW_ALWAYS_INLINE explicit NaNBoxedVariant(std::in_place_type_t<T>, CArgs&&... args) noexcept {
        using UT = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<UT, flat_list>::value;
        // For trivial types, just construct temp and encode
        UT tmp(std::forward<CArgs>(args)...);
        bits_ = encode_type<UT>(idx, std::move(tmp));
    }

    // In-place index
    template <std::size_t I, typename... CArgs>
    MEOW_ALWAYS_INLINE explicit NaNBoxedVariant(std::in_place_index_t<I>, CArgs&&... args) noexcept {
        static_assert(I < alternatives_count, "in_place_index out of range");
        using U = typename detail::nth_type<I, flat_list>::type;
        U tmp(std::forward<CArgs>(args)...);
        bits_ = encode_type<U>(I, std::move(tmp));
    }

    // --- Assignment ---
    MEOW_ALWAYS_INLINE NaNBoxedVariant& operator=(const NaNBoxedVariant& o) noexcept {
        bits_ = o.bits_;
        return *this;
    }
    MEOW_ALWAYS_INLINE NaNBoxedVariant& operator=(NaNBoxedVariant&& o) noexcept {
        bits_ = o.bits_;
        o.bits_ = MEOW_VALUELESS_BITS;
        return *this;
    }

    template <typename T, typename U = std::decay_t<T>, 
              typename = std::enable_if_t<(detail::type_list_index_of<U, flat_list>::value != detail::invalid_index)>>
    MEOW_ALWAYS_INLINE NaNBoxedVariant& operator=(T&& v) noexcept {
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        bits_ = encode_type<U>(idx, std::forward<T>(v));
        return *this;
    }

    // --- Emplace ---
    template <typename T, typename... CArgs, typename U = std::decay_t<T>, 
              typename = std::enable_if_t<(detail::type_list_index_of<U, flat_list>::value != detail::invalid_index)>>
    MEOW_ALWAYS_INLINE void emplace(CArgs&&... args) noexcept {
        using UT = std::decay_t<T>;
        constexpr std::size_t idx = detail::type_list_index_of<UT, flat_list>::value;
        UT tmp(std::forward<CArgs>(args)...);
        bits_ = encode_type<UT>(idx, std::move(tmp));
    }

    template <std::size_t I, typename... CArgs>
    MEOW_ALWAYS_INLINE void emplace_index(CArgs&&... args) noexcept {
        static_assert(I < alternatives_count, "emplace_index out of range");
        using U = typename detail::nth_type<I, flat_list>::type;
        U tmp(std::forward<CArgs>(args)...);
        bits_ = encode_type<U>(I, std::move(tmp));
    }

    // --- Emplace or Assign ---
    template <typename T, typename... CArgs>
    MEOW_ALWAYS_INLINE void emplace_or_assign(CArgs&&... args) noexcept {
        emplace<T>(std::forward<CArgs>(args)...);
    }

    // --- Observers ---
    [[nodiscard]] MEOW_ALWAYS_INLINE std::size_t index() const noexcept {
        if (bits_ == MEOW_VALUELESS_BITS) return npos;
        
        // If it's a raw double (not NaN), it must be the double type
        if (meow_is_raw_double(bits_)) return dbl_idx;

        // Extract tag (bits 48-50)
        uint8_t tag = static_cast<uint8_t>((bits_ >> MEOW_TAG_SHIFT) & 0x7);
        
        // Mapping: Tag N -> Index N.
        // Note: Standard NaN (0x7FF8...) has Tag 0.
        // If 'double' is Index 0, its NaN form is Tag 0.
        // If 'double' is Index 1, its NaN form is encoded as Tag 1.
        // If 'int' is Index 0, it is encoded as Tag 0.
        return static_cast<std::size_t>(tag);
    }

    [[nodiscard]] MEOW_ALWAYS_INLINE bool valueless() const noexcept {
        return bits_ == MEOW_VALUELESS_BITS;
    }

    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE bool holds() const noexcept {
        constexpr std::size_t idx = detail::type_list_index_of<std::decay_t<T>, flat_list>::value;
        if (idx == detail::invalid_index) return false;
        return index() == idx;
    }
    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE bool is() const noexcept {
        return holds<T>();
    }

    // --- Getters ---
    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE std::decay_t<T> get() noexcept {
        return decode_value<std::decay_t<T>>(bits_);
    }
    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE std::decay_t<T> get() const noexcept {
        return decode_value<std::decay_t<T>>(bits_);
    }

    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE std::decay_t<T> safe_get() const {
        if (MEOW_UNLIKELY(!holds<T>())) throw std::bad_variant_access();
        return decode_value<std::decay_t<T>>(bits_);
    }

    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE std::decay_t<T>* get_if() noexcept {
        if (MEOW_UNLIKELY(!holds<T>())) return nullptr;
        // Note: returning pointer to static thread_local is common hack for by-value storage
        // but be careful with lifetime/concurrency if T is mutable. 
        // Since T is trivial here (int/ptr/double), by-value return is preferred usually.
        // To support get_if signature, we use a temporary.
        thread_local static std::decay_t<T> temp;
        temp = decode_value<std::decay_t<T>>(bits_);
        return &temp;
    }
    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE const std::decay_t<T>* get_if() const noexcept {
        // const_cast to reuse the non-const version's logic or duplicate
        return const_cast<NaNBoxedVariant*>(this)->get_if<T>();
    }

    // --- Visit ---
    template <typename Visitor>
    MEOW_ALWAYS_INLINE decltype(auto) visit(Visitor&& vis) {
        std::size_t idx = index();
        if (MEOW_UNLIKELY(idx == npos)) throw std::bad_variant_access();
        return visit_impl(std::forward<Visitor>(vis), idx);
    }
    template <typename Visitor>
    MEOW_ALWAYS_INLINE decltype(auto) visit(Visitor&& vis) const {
        std::size_t idx = index();
        if (MEOW_UNLIKELY(idx == npos)) throw std::bad_variant_access();
        return visit_impl(std::forward<Visitor>(vis), idx); // decode value is same
    }

    // --- Swap ---
    MEOW_ALWAYS_INLINE void swap(NaNBoxedVariant& o) noexcept {
        uint64_t tmp = bits_;
        bits_ = o.bits_;
        o.bits_ = tmp;
    }

    // --- Raw Access ---
    [[nodiscard]] MEOW_ALWAYS_INLINE uint64_t get_raw_bits() const noexcept { return bits_; }
    MEOW_ALWAYS_INLINE void set_raw_bits(uint64_t b) noexcept { bits_ = b; }

    MEOW_ALWAYS_INLINE bool operator==(const NaNBoxedVariant& o) const noexcept {
        return bits_ == o.bits_;
    }
    MEOW_ALWAYS_INLINE bool operator!=(const NaNBoxedVariant& o) const noexcept {
        return !(*this == o);
    }

   private:
    uint64_t bits_;

    // --- Encoding Logic ---
    template <typename T>
    MEOW_ALWAYS_INLINE static uint64_t encode_type(std::size_t idx, T&& v) noexcept {
        using U = std::decay_t<T>;
        if constexpr (is_double_like<U>::value) {
            uint64_t b = meow_bitcast_double_to_u64(static_cast<double>(v));
            // If it is a raw double (not NaN), store as is.
            // If it is a NaN, we must ensure it has the correct Tag corresponding to idx.
            if (meow_is_raw_double(b)) {
                // Check for Sentinel collision (extremely rare: specific -NaN)
                // If collision, flip LSB of payload to make it slightly different NaN
                if (MEOW_UNLIKELY(b == MEOW_VALUELESS_BITS)) {
                    b ^= 1ULL; 
                }
                return b;
            } else {
                // It is a NaN. Re-encode with correct tag.
                // Preserve payload (lower 48 bits).
                return MEOW_QNAN_PREFIX | (static_cast<uint64_t>(idx) << MEOW_TAG_SHIFT) | (b & MEOW_PAYLOAD_MASK);
            }
        } else {
            // Non-double types
            uint64_t payload = 0;
            if constexpr (is_pointer_like<U>::value) {
                payload = meow_payload_from_ptr(reinterpret_cast<const void*>(v));
            } else if constexpr (is_integral_like<U>::value) {
                payload = static_cast<uint64_t>(static_cast<int64_t>(v)) & MEOW_PAYLOAD_MASK;
            } else if constexpr (is_bool_like<U>::value) {
                payload = v ? 1ULL : 0ULL;
            }
            // Encode: Prefix + Tag + Payload
            return MEOW_QNAN_PREFIX | (static_cast<uint64_t>(idx) << MEOW_TAG_SHIFT) | payload;
        }
    }

    // --- Decoding Logic ---
    template <typename T>
    MEOW_ALWAYS_INLINE static T decode_value(uint64_t bits) noexcept {
        using U = std::decay_t<T>;
        if constexpr (is_double_like<U>::value) {
            // If it was stored as raw double, bitcast back.
            // If it was a NaN (tagged), bitcast back (it's still a NaN).
            return meow_bitcast_u64_to_double(bits);
        } else {
            uint64_t payload = bits & MEOW_PAYLOAD_MASK;
            if constexpr (is_pointer_like<U>::value) {
                return reinterpret_cast<U>(meow_ptr_from_payload(payload));
            } else if constexpr (is_integral_like<U>::value) {
                // Sign-extend if needed (if bit 47 is set)
                if (payload & (1ULL << 47)) {
                    return static_cast<U>(static_cast<int64_t>(payload | 0xFFFF000000000000ULL));
                } else {
                    return static_cast<U>(static_cast<int64_t>(payload));
                }
            } else if constexpr (is_bool_like<U>::value) {
                return static_cast<U>(payload != 0);
            } else {
                return U{};
            }
        }
    }

    // --- Visitor Jump Table ---
    template <typename Visitor, std::size_t... Is>
    MEOW_ALWAYS_INLINE decltype(auto) visit_impl_helper(Visitor&& vis, std::size_t idx, std::index_sequence<Is...>) const {
        using R = std::invoke_result_t<Visitor, typename detail::nth_type<0, flat_list>::type>; // Approximate return type
        using fn_t = R (*)(uint64_t, Visitor&&);

        static constexpr std::array<fn_t, alternatives_count> fns = {{+[](uint64_t bits, Visitor&& vis2) -> R {
            using T = typename detail::nth_type<Is, flat_list>::type;
            return std::forward<Visitor>(vis2)(decode_value<T>(bits));
        }...}};

        // Note: idx is guaranteed < alternatives_count by caller logic (unless corruption)
        return fns[idx](bits_, std::forward<Visitor>(vis));
    }

    template <typename Visitor>
    MEOW_ALWAYS_INLINE decltype(auto) visit_impl(Visitor&& vis, std::size_t idx) const {
        return visit_impl_helper(std::forward<Visitor>(vis), idx, std::make_index_sequence<alternatives_count>{});
    }
};

// Non-member swap
template <typename... Ts>
MEOW_ALWAYS_INLINE void swap(NaNBoxedVariant<Ts...>& a, NaNBoxedVariant<Ts...>& b) noexcept {
    a.swap(b);
}

}  // namespace meow::utils