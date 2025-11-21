#pragma once

#include "common/pch.h"

namespace meow::inline utils::inline string {
    struct StringHash {
        using is_transparent = void;
        
        static size_t operator()(const char* txt) noexcept { return std::hash<std::string_view>{}(txt); }
        static size_t operator()(std::string_view txt) noexcept { return std::hash<std::string_view>{}(txt); }
        static size_t operator()(const std::string& txt) noexcept { return std::hash<std::string>{}(txt); }
    };
}