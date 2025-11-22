#pragma once

#include "common/pch.h"

namespace meow::inline runtime { class Chunk; }

namespace meow::inline debug {
std::string disassemble_chunk(const meow::Chunk& chunk) noexcept;
} // namespace meow::debug