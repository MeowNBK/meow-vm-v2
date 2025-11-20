#pragma once

#include <array>        // std::array
#include <bit>          // std::bit_cast (C++20)
#include <cmath>        // std::isnan
#include <cstddef>      // std::size_t
#include <cstdint>      // uint64_t
#include <limits>       // std::numeric_limits
#include <stdexcept>    // std::bad_variant_access
#include <type_traits>  // std::decay_t, std::is_same
#include <utility>      // std::forward, std::move
#include <variant>      // std::monostate

#include "variant_utils.h"

namespace meow::utils {

// --- Cấu hình Platform (Chỉ chạy trên 64-bit Little Endian) ---
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__))
    #define MEOW_LITTLE_64 1
#else
    #define MEOW_LITTLE_64 0
#endif

static_assert(MEOW_LITTLE_64, "NaN-boxing requires 64-bit little-endian CPU.");

// Macro tối ưu nhánh (Branch prediction hints)
#if defined(__GNUC__) || defined(__clang__)
    #define MEOW_ALWAYS_INLINE inline __attribute__((always_inline))
    #define MEOW_LIKELY(x) (__builtin_expect(!!(x), 1))
    #define MEOW_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
    #define MEOW_ALWAYS_INLINE inline
    #define MEOW_LIKELY(x) (x)
    #define MEOW_UNLIKELY(x) (x)
#endif

// --- Classifiers (Phân loại kiểu dữ liệu) ---
template <typename T> struct is_pointer_like { static constexpr bool value = std::is_pointer_v<std::decay_t<T>>; };
template <typename T> struct is_integral_like { static constexpr bool value = std::is_integral_v<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, bool>; };
template <typename T> struct is_double_like { static constexpr bool value = std::is_floating_point_v<std::decay_t<T>>; };
template <typename T> struct is_bool_like { static constexpr bool value = std::is_same_v<std::decay_t<T>, bool>; };

// Kiểm tra xem tất cả các type có nhét vừa vào NaN-box không
template <typename List> struct all_nanboxable_impl;
template <> struct all_nanboxable_impl<detail::type_list<>> : std::true_type {};
template <typename H, typename... Ts>
struct all_nanboxable_impl<detail::type_list<H, Ts...>>
    : std::integral_constant<bool, (sizeof(std::decay_t<H>) <= 8) && 
      (is_double_like<H>::value || is_integral_like<H>::value || is_pointer_like<H>::value || 
       std::is_same_v<std::decay_t<H>, std::monostate> || is_bool_like<H>::value) &&
      all_nanboxable_impl<detail::type_list<Ts...>>::value> {};

// --- Các hằng số NaN-boxing ---
// Double chuẩn: [Sign:1] [Exp:11] [Mantissa:52]
// QNaN (Quiet NaN): Exp=0x7FF, Bit cao nhất của Mantissa=1.
static constexpr uint64_t MEOW_EXP_MASK     = 0x7FF0000000000000ULL;
static constexpr uint64_t MEOW_QNAN_PREFIX  = 0x7FF8000000000000ULL; 
static constexpr uint64_t MEOW_TAG_MASK     = 0x0007000000000000ULL; // Bits 48-50 (3 bit Tag)
static constexpr uint64_t MEOW_PAYLOAD_MASK = 0x0000FFFFFFFFFFFFULL; // 48 bit Payload
static constexpr unsigned MEOW_TAG_SHIFT    = 48;
static constexpr uint64_t MEOW_VALUELESS    = 0xFFFFFFFFFFFFFFFFULL; // Sentinel cho trạng thái rỗng

// --- Hàm helper ---
MEOW_ALWAYS_INLINE uint64_t to_bits(double d) { return std::bit_cast<uint64_t>(d); }
MEOW_ALWAYS_INLINE double from_bits(uint64_t u) { return std::bit_cast<double>(u); }
MEOW_ALWAYS_INLINE bool is_double(uint64_t b) { return (b & MEOW_EXP_MASK) != MEOW_EXP_MASK; }

// ============================================================================
// CLASS NaNBoxedVariant (Kích thước: 8 bytes)
// ============================================================================
template <typename... Args>
class NaNBoxedVariant {
    using flat_list = detail::flattened_unique_t<Args...>;
    static constexpr std::size_t count = detail::type_list_length<flat_list>::value;
    static constexpr std::size_t dbl_idx = detail::type_list_index_of<double, flat_list>::value;
    
    static_assert(count <= 8, "NaNBoxedVariant supports max 8 types (3-bit tag).");
    static_assert(all_nanboxable_impl<flat_list>::value, "All types must fit in 8 bytes.");

public:
    using inner_types = flat_list;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    // --- Constructors ---
    MEOW_ALWAYS_INLINE NaNBoxedVariant() noexcept {
        if constexpr (std::is_same_v<typename detail::nth_type<0, flat_list>::type, std::monostate>) {
            bits_ = MEOW_QNAN_PREFIX; // Tag 0 (Monostate), Payload 0
        } else {
            bits_ = MEOW_VALUELESS;
        }
    }
    
    MEOW_ALWAYS_INLINE NaNBoxedVariant(const NaNBoxedVariant& o) noexcept : bits_(o.bits_) {}
    MEOW_ALWAYS_INLINE NaNBoxedVariant(NaNBoxedVariant&& o) noexcept : bits_(o.bits_) { o.bits_ = MEOW_VALUELESS; }

    // Constructor nhận giá trị (Quan trọng: dùng std::decay_t để lọc kiểu)
    template <typename T, typename U = std::decay_t<T>, 
              typename = std::enable_if_t<(detail::type_list_index_of<U, flat_list>::value != detail::invalid_index)>>
    MEOW_ALWAYS_INLINE NaNBoxedVariant(T&& v) noexcept {
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        bits_ = encode<U>(idx, std::forward<T>(v));
    }

    // --- Assignment ---
    MEOW_ALWAYS_INLINE NaNBoxedVariant& operator=(const NaNBoxedVariant& o) noexcept { bits_ = o.bits_; return *this; }
    MEOW_ALWAYS_INLINE NaNBoxedVariant& operator=(NaNBoxedVariant&& o) noexcept {
        bits_ = o.bits_; o.bits_ = MEOW_VALUELESS; return *this;
    }

    template <typename T, typename U = std::decay_t<T>,
              typename = std::enable_if_t<(detail::type_list_index_of<U, flat_list>::value != detail::invalid_index)>>
    MEOW_ALWAYS_INLINE NaNBoxedVariant& operator=(T&& v) noexcept {
        constexpr std::size_t idx = detail::type_list_index_of<U, flat_list>::value;
        bits_ = encode<U>(idx, std::forward<T>(v));
        return *this;
    }

    // --- Observers ---
    [[nodiscard]] MEOW_ALWAYS_INLINE std::size_t index() const noexcept {
        if (bits_ == MEOW_VALUELESS) return npos;
        if (is_double(bits_)) return dbl_idx;
        // Lấy 3 bit Tag ra:
        return static_cast<std::size_t>((bits_ >> MEOW_TAG_SHIFT) & 0x7);
    }

    [[nodiscard]] MEOW_ALWAYS_INLINE bool valueless() const noexcept { return bits_ == MEOW_VALUELESS; }

    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE bool holds() const noexcept {
        constexpr std::size_t idx = detail::type_list_index_of<std::decay_t<T>, flat_list>::value;
        return index() == idx;
    }

    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE bool is() const noexcept { return holds<T>(); }

    // --- Getters ---
    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE std::decay_t<T> get() const noexcept {
        return decode<std::decay_t<T>>(bits_);
    }

    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE std::decay_t<T> safe_get() const {
        if (MEOW_UNLIKELY(!holds<T>())) throw std::bad_variant_access();
        return decode<std::decay_t<T>>(bits_);
    }

    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE std::decay_t<T>* get_if() noexcept {
        if (!holds<T>()) return nullptr;
        // Hack nhỏ để trả về pointer cho giá trị primitives (vốn ko có địa chỉ trong register)
        thread_local static std::decay_t<T> temp; 
        temp = decode<std::decay_t<T>>(bits_);
        return &temp;
    }
    
    template <typename T>
    [[nodiscard]] MEOW_ALWAYS_INLINE const std::decay_t<T>* get_if() const noexcept {
        return const_cast<NaNBoxedVariant*>(this)->get_if<T>();
    }

    // --- Utilities ---
    MEOW_ALWAYS_INLINE void swap(NaNBoxedVariant& o) noexcept { 
        uint64_t t = bits_; bits_ = o.bits_; o.bits_ = t; 
    }
    [[nodiscard]] MEOW_ALWAYS_INLINE uint64_t get_raw_bits() const noexcept { return bits_; }
    
    MEOW_ALWAYS_INLINE bool operator==(const NaNBoxedVariant& o) const { return bits_ == o.bits_; }
    MEOW_ALWAYS_INLINE bool operator!=(const NaNBoxedVariant& o) const { return bits_ != o.bits_; }

    // --- Visit ---
    template <typename Visitor>
    MEOW_ALWAYS_INLINE decltype(auto) visit(Visitor&& vis) const {
        std::size_t idx = index();
        if (MEOW_UNLIKELY(idx == npos)) throw std::bad_variant_access();
        return visit_impl(std::forward<Visitor>(vis), idx);
    }
    
    template <typename Visitor>
    MEOW_ALWAYS_INLINE decltype(auto) visit(Visitor&& vis) {
        return std::as_const(*this).visit(std::forward<Visitor>(vis));
    }

private:
    uint64_t bits_;

    // --- Encoding Logic (Đã fix lỗi lvalue/rvalue bằng cách pass-by-value) ---
    // Pass-by-value cho các kiểu cơ bản (int, double, ptr) thực ra nhanh hơn tham chiếu
    template <typename T>
    MEOW_ALWAYS_INLINE static uint64_t encode(std::size_t idx, T v) noexcept {
        using U = std::decay_t<T>;
        if constexpr (is_double_like<U>::value) {
            uint64_t b = to_bits(static_cast<double>(v));
            // Nếu double này trùng khớp với cấu trúc NaN-box của ta (rất hiếm), 
            // ta phải "làm bẩn" nó 1 chút để tránh nhầm lẫn.
            if (!is_double(b)) return b; // Nó đã là NaN rồi, giữ nguyên payload
            if (MEOW_UNLIKELY(b == MEOW_VALUELESS)) return b ^ 1; // Tránh trùng Sentinel
            return b; // Double sạch
        } else {
            uint64_t payload = 0;
            if constexpr (is_pointer_like<U>::value) {
                payload = reinterpret_cast<uintptr_t>(static_cast<const void*>(v));
            } else if constexpr (is_integral_like<U>::value) {
                payload = static_cast<uint64_t>(static_cast<int64_t>(v));
            } else if constexpr (is_bool_like<U>::value) {
                payload = v ? 1 : 0;
            }
            // Cấu trúc: [Prefix QNaN] | [Tag 3-bit] | [Payload 48-bit]
            return MEOW_QNAN_PREFIX | (static_cast<uint64_t>(idx) << MEOW_TAG_SHIFT) | (payload & MEOW_PAYLOAD_MASK);
        }
    }

    // --- Decoding Logic (Tối ưu Bitwise) ---
    template <typename T>
    MEOW_ALWAYS_INLINE static T decode(uint64_t bits) noexcept {
        using U = std::decay_t<T>;
        if constexpr (is_double_like<U>::value) {
            return from_bits(bits);
        } else if constexpr (is_integral_like<U>::value) {
            // Kỹ thuật dịch bit để khôi phục dấu (Sign extension) cực nhanh:
            // Đẩy bit dấu từ vị trí 47 lên 63, rồi kéo ngược về để nó phủ hết phần bit cao.
            return static_cast<U>(static_cast<int64_t>(bits << 16) >> 16);
        } else if constexpr (is_pointer_like<U>::value) {
            return reinterpret_cast<U>(static_cast<uintptr_t>(bits & MEOW_PAYLOAD_MASK));
        } else if constexpr (is_bool_like<U>::value) {
            return static_cast<U>((bits & MEOW_PAYLOAD_MASK) != 0);
        } else {
            return U{};
        }
    }

    // --- Visit Jump Table ---
    template <typename Visitor, std::size_t... Is>
    MEOW_ALWAYS_INLINE decltype(auto) visit_impl_helper(Visitor&& vis, std::size_t idx, std::index_sequence<Is...>) const {
        using R = std::invoke_result_t<Visitor, typename detail::nth_type<0, flat_list>::type>;
        using fn_t = R (*)(uint64_t, Visitor&&);
        
        static constexpr std::array<fn_t, count> table = {{
            +[](uint64_t b, Visitor&& v) -> R {
                return std::forward<Visitor>(v)(decode<typename detail::nth_type<Is, flat_list>::type>(b));
            }...
        }};
        return table[idx](bits_, std::forward<Visitor>(vis));
    }

    template <typename Visitor>
    MEOW_ALWAYS_INLINE decltype(auto) visit_impl(Visitor&& vis, std::size_t idx) const {
        return visit_impl_helper(std::forward<Visitor>(vis), idx, std::make_index_sequence<count>{});
    }
};

template <typename... Ts>
MEOW_ALWAYS_INLINE void swap(NaNBoxedVariant<Ts...>& a, NaNBoxedVariant<Ts...>& b) noexcept { a.swap(b); }

} // namespace meow::utils