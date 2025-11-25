add_library(meow_variant INTERFACE)

target_include_directories(meow_variant INTERFACE 
    # 1. Để user thấy "meow_variant.h" (trong folder include)
    ${CMAKE_CURRENT_SOURCE_DIR}/include

    # 2. MAGIC: Để code bên trong có thể gọi "internal/..." mà ko cần ".."
    # Thêm chính thư mục hiện tại (libs/meow_variant) vào đường dẫn tìm kiếm
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(meow_variant INTERFACE cxx_std_23)
add_library(meow::variant ALIAS meow_variant)