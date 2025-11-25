#pragma once

#include "common/pch.h"

namespace meow { class Chunk; }

namespace meow {
std::string disassemble_chunk(const meow::Chunk& chunk) noexcept;
} // namespace meow::debug