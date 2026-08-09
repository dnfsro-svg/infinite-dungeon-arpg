#pragma once

#include <cstddef>

namespace arpg::platform {

struct UiUtf8CopyResult final {
    std::size_t bytes_written{};
    // For format(), this reports validation of bytes materialized in the
    // fixed destination. Bytes omitted by printf truncation are not observed.
    bool input_was_valid{};
    bool truncated{};
};

[[nodiscard]] bool ui_utf8_is_valid(const char* text) noexcept;

// Malformed input consumes exactly one input byte and writes one ASCII '?',
// allowing following ASCII and valid UTF-8 bytes to be recovered.
[[nodiscard]] UiUtf8CopyResult ui_utf8_copy(
    char* destination, std::size_t capacity, const char* source) noexcept;

// On truncation, writes the longest whole-code-point prefix followed by UTF-8
// ellipsis '…'. If the destination cannot hold the complete ellipsis, writes
// an empty NUL-terminated string instead.
[[nodiscard]] UiUtf8CopyResult ui_utf8_fit_with_ellipsis(
    char* destination, std::size_t capacity, const char* source) noexcept;

[[nodiscard]] UiUtf8CopyResult ui_utf8_format(
    char* destination, std::size_t capacity, const char* format, ...) noexcept;

}  // namespace arpg::platform
