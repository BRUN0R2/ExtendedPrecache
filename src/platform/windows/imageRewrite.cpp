#include "imageRewrite.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ranges>
#include <utility>

namespace extendedPrecache::platform::windows {
namespace {
auto readOperand(const std::byte *address) noexcept -> std::uint32_t {
    auto value = std::uint32_t{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

[[nodiscard]] auto writeOperand(std::byte *address, std::uint32_t value) noexcept -> bool {
    auto previous = DWORD{};
    if (VirtualProtect(address, sizeof(value), PAGE_EXECUTE_READWRITE, &previous) == FALSE) {
        return false;
    }
    std::memcpy(address, &value, sizeof(value));
    auto replaced = DWORD{};
    const auto restored = VirtualProtect(address, sizeof(value), previous, &replaced) != FALSE;
    return FlushInstructionCache(GetCurrentProcess(), address, sizeof(value)) != FALSE && restored;
}
} // namespace

auto ImageRewrite::apply(std::byte *image, std::vector<OperandRewrite> rewrites) noexcept
    -> std::expected<ImageRewrite, RewriteRefusal> {
    const auto holdsExpected = [image](const OperandRewrite &rewrite) {
        return readOperand(image + rewrite.operandRva) == rewrite.expected;
    };
    if (!std::ranges::all_of(rewrites, holdsExpected)) {
        return std::unexpected{RewriteRefusal::unexpectedCode};
    }
    auto applied = ImageRewrite{image, std::move(rewrites)};
    for (const auto &rewrite : applied.rewrites) {
        if (!writeOperand(image + rewrite.operandRva, rewrite.replacement)) {
            return std::unexpected{RewriteRefusal::notWritable};
        }
        ++applied.written;
    }
    return applied;
}

ImageRewrite::ImageRewrite(ImageRewrite &&other) noexcept
    : image{std::exchange(other.image, nullptr)}, rewrites{std::move(other.rewrites)},
      written{std::exchange(other.written, 0)} {}

ImageRewrite::~ImageRewrite() {
    for (const auto &rewrite : rewrites | std::views::take(written)) {
        if (!writeOperand(image + rewrite.operandRva, rewrite.expected)) {
            // The code would go on using storage its owner is about to free.
            std::abort();
        }
    }
}

} // namespace extendedPrecache::platform::windows
