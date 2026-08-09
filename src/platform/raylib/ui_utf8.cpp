#include "ui_utf8.hpp"

#include <cstdarg>
#include <cstdint>
#include <cstdio>

namespace arpg::platform {
namespace {

enum class DecodeKind : unsigned char {
    valid,
    invalid,
    incomplete,
};

struct DecodedCodePoint final {
    DecodeKind kind{DecodeKind::invalid};
    std::size_t length{1U};
};

[[nodiscard]] bool is_continuation(unsigned char byte) noexcept {
    return byte >= 0x80U && byte <= 0xBFU;
}

[[nodiscard]] DecodedCodePoint decode(const char* text) noexcept {
    const auto first = static_cast<unsigned char>(text[0]);
    if (first <= 0x7FU) return {DecodeKind::valid, 1U};

    if (first >= 0xC2U && first <= 0xDFU) {
        const auto second = static_cast<unsigned char>(text[1]);
        return is_continuation(second)
            ? DecodedCodePoint{DecodeKind::valid, 2U}
            : DecodedCodePoint{second == 0U ? DecodeKind::incomplete
                                             : DecodeKind::invalid,
                1U};
    }

    if (first >= 0xE0U && first <= 0xEFU) {
        const auto second = static_cast<unsigned char>(text[1]);
        if (second == 0U) return {DecodeKind::incomplete, 1U};
        const bool valid_second = is_continuation(second)
            && !(first == 0xE0U && second < 0xA0U)
            && !(first == 0xEDU && second > 0x9FU);
        if (!valid_second) return {DecodeKind::invalid, 1U};
        const auto third = static_cast<unsigned char>(text[2]);
        return is_continuation(third)
            ? DecodedCodePoint{DecodeKind::valid, 3U}
            : DecodedCodePoint{third == 0U ? DecodeKind::incomplete
                                            : DecodeKind::invalid,
                1U};
    }

    if (first >= 0xF0U && first <= 0xF4U) {
        const auto second = static_cast<unsigned char>(text[1]);
        if (second == 0U) return {DecodeKind::incomplete, 1U};
        const bool valid_second = is_continuation(second)
            && !(first == 0xF0U && second < 0x90U)
            && !(first == 0xF4U && second > 0x8FU);
        if (!valid_second) return {DecodeKind::invalid, 1U};
        const auto third = static_cast<unsigned char>(text[2]);
        if (third == 0U) return {DecodeKind::incomplete, 1U};
        if (!is_continuation(third)) return {DecodeKind::invalid, 1U};
        const auto fourth = static_cast<unsigned char>(text[3]);
        return is_continuation(fourth)
            ? DecodedCodePoint{DecodeKind::valid, 4U}
            : DecodedCodePoint{fourth == 0U ? DecodeKind::incomplete
                                             : DecodeKind::invalid,
                1U};
    }

    return {DecodeKind::invalid, 1U};
}

[[nodiscard]] std::size_t writable_capacity(
    const char* destination, std::size_t capacity) noexcept {
    return destination == nullptr || capacity == 0U ? 0U : capacity - 1U;
}

enum class BufferOverlap : unsigned char {
    disjoint,
    exact,
    partial,
};

[[nodiscard]] BufferOverlap buffer_overlap(
    const char* destination, std::size_t capacity, const char* source) noexcept {
    if (destination == nullptr || capacity == 0U || source == nullptr) {
        return BufferOverlap::disjoint;
    }
    if (destination == source) return BufferOverlap::exact;

    std::uintptr_t source_bytes{1U};
    for (const char* cursor = source; *cursor != '\0'; ++cursor) {
        ++source_bytes;
    }
    const auto destination_address = reinterpret_cast<std::uintptr_t>(destination);
    const auto source_address = reinterpret_cast<std::uintptr_t>(source);
    if (destination_address < source_address) {
        return source_address - destination_address
                < static_cast<std::uintptr_t>(capacity)
            ? BufferOverlap::partial
            : BufferOverlap::disjoint;
    }
    return destination_address - source_address < source_bytes
        ? BufferOverlap::partial
        : BufferOverlap::disjoint;
}

void write_bytes(char* destination, std::size_t& written,
    const char* source, std::size_t length) noexcept {
    for (std::size_t index{}; index < length; ++index) {
        destination[written++] = source[index];
    }
}

void terminate(char* destination, std::size_t capacity,
    std::size_t written) noexcept {
    if (destination != nullptr && capacity != 0U) {
        destination[written] = '\0';
    }
}

[[nodiscard]] UiUtf8CopyResult reject_partial_overlap(
    char* destination, std::size_t capacity, const char* source) noexcept {
    UiUtf8CopyResult result{};
    result.input_was_valid = ui_utf8_is_valid(source);
    result.truncated = source[0] != '\0';
    terminate(destination, capacity, 0U);
    return result;
}

struct Utf8Scan final {
    bool input_was_valid{true};
    bool fits{};
    bool has_output{};
};

[[nodiscard]] Utf8Scan scan_for_capacity(
    const char* source, std::size_t capacity) noexcept {
    Utf8Scan scan{};
    if (source == nullptr) {
        scan.input_was_valid = false;
        scan.fits = true;
        return scan;
    }

    std::size_t used{};
    scan.fits = true;
    for (const char* cursor = source; *cursor != '\0';) {
        const DecodedCodePoint decoded = decode(cursor);
        const std::size_t output_length = decoded.kind == DecodeKind::valid
            ? decoded.length
            : 1U;
        scan.input_was_valid = scan.input_was_valid
            && decoded.kind == DecodeKind::valid;
        scan.has_output = true;
        if (output_length > capacity - used) {
            scan.fits = false;
        } else {
            used += output_length;
        }
        cursor += decoded.length;
    }
    return scan;
}

void write_transformed_prefix(char* destination, std::size_t& written,
    std::size_t capacity, const char* source) noexcept {
    for (const char* cursor = source; *cursor != '\0';) {
        const DecodedCodePoint decoded = decode(cursor);
        const std::size_t output_length = decoded.kind == DecodeKind::valid
            ? decoded.length
            : 1U;
        if (output_length > capacity - written) break;
        if (decoded.kind == DecodeKind::valid) {
            write_bytes(destination, written, cursor, decoded.length);
        } else {
            destination[written++] = '?';
        }
        cursor += decoded.length;
    }
}

[[nodiscard]] UiUtf8CopyResult repair_formatted_tail(
    char* destination, std::size_t capacity, bool format_was_truncated) noexcept {
    UiUtf8CopyResult result{};
    result.input_was_valid = true;
    const std::size_t writable = writable_capacity(destination, capacity);
    std::size_t written{};

    for (const char* cursor = destination; *cursor != '\0';) {
        const DecodedCodePoint decoded = decode(cursor);
        if (decoded.kind == DecodeKind::valid) {
            write_bytes(destination, written, cursor, decoded.length);
            cursor += decoded.length;
            continue;
        }
        if (format_was_truncated && decoded.kind == DecodeKind::incomplete) {
            result.truncated = true;
            break;
        }
        destination[written++] = '?';
        result.input_was_valid = false;
        cursor += decoded.length;
    }

    terminate(destination, capacity, written);
    result.bytes_written = written;
    result.truncated = result.truncated || written == writable
        && format_was_truncated;
    return result;
}

}  // namespace

bool ui_utf8_is_valid(const char* text) noexcept {
    if (text == nullptr) return false;
    for (const char* cursor = text; *cursor != '\0';) {
        const DecodedCodePoint decoded = decode(cursor);
        if (decoded.kind != DecodeKind::valid) return false;
        cursor += decoded.length;
    }
    return true;
}

UiUtf8CopyResult ui_utf8_copy(
    char* destination, std::size_t capacity, const char* source) noexcept {
    UiUtf8CopyResult result{};
    result.input_was_valid = source != nullptr;
    const std::size_t writable = writable_capacity(destination, capacity);
    std::size_t written{};
    if (source == nullptr) {
        terminate(destination, capacity, written);
        return result;
    }
    if (buffer_overlap(destination, capacity, source)
        == BufferOverlap::partial) {
        return reject_partial_overlap(destination, capacity, source);
    }

    for (const char* cursor = source; *cursor != '\0';) {
        const DecodedCodePoint decoded = decode(cursor);
        const std::size_t output_length = decoded.kind == DecodeKind::valid
            ? decoded.length
            : 1U;
        result.input_was_valid = result.input_was_valid
            && decoded.kind == DecodeKind::valid;
        if (output_length > writable - written) {
            result.truncated = true;
        } else if (decoded.kind == DecodeKind::valid) {
            write_bytes(destination, written, cursor, decoded.length);
        } else {
            destination[written++] = '?';
        }
        cursor += decoded.length;
    }
    terminate(destination, capacity, written);
    result.bytes_written = written;
    return result;
}

UiUtf8CopyResult ui_utf8_fit_with_ellipsis(
    char* destination, std::size_t capacity, const char* source) noexcept {
    if (buffer_overlap(destination, capacity, source)
        == BufferOverlap::partial) {
        return reject_partial_overlap(destination, capacity, source);
    }
    const std::size_t writable = writable_capacity(destination, capacity);
    Utf8Scan scan = scan_for_capacity(source, writable);
    UiUtf8CopyResult result{};
    result.input_was_valid = scan.input_was_valid;
    if (scan.fits) return ui_utf8_copy(destination, capacity, source);

    result.truncated = scan.has_output;
    constexpr char kEllipsis[] = u8"…";
    constexpr std::size_t kEllipsisBytes = sizeof(kEllipsis) - 1U;
    if (destination == nullptr || writable < kEllipsisBytes || source == nullptr) {
        terminate(destination, capacity, 0U);
        return result;
    }

    std::size_t written{};
    write_transformed_prefix(destination, written, writable - kEllipsisBytes,
        source);
    write_bytes(destination, written, kEllipsis, kEllipsisBytes);
    terminate(destination, capacity, written);
    result.bytes_written = written;
    return result;
}

UiUtf8CopyResult ui_utf8_format(
    char* destination, std::size_t capacity, const char* format, ...) noexcept {
    UiUtf8CopyResult result{};
    if (format == nullptr) {
        terminate(destination, capacity, 0U);
        return result;
    }

    va_list arguments;
    va_start(arguments, format);
    const std::size_t format_capacity = destination == nullptr ? 0U : capacity;
    const int formatted = std::vsnprintf(
        destination, format_capacity, format, arguments);
    va_end(arguments);
    if (formatted < 0) {
        terminate(destination, capacity, 0U);
        return result;
    }

    if (format_capacity == 0U) {
        result.input_was_valid = true;
        result.truncated = formatted > 0;
        return result;
    }

    destination[capacity - 1U] = '\0';
    const bool format_was_truncated = static_cast<std::size_t>(formatted)
        >= capacity;
    result = repair_formatted_tail(destination, capacity, format_was_truncated);
    result.truncated = result.truncated || format_was_truncated;
    return result;
}

}  // namespace arpg::platform
