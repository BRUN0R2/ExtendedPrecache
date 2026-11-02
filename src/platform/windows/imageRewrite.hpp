#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>
#include <vector>

namespace extendedPrecache::platform::windows {

// A 32-bit operand of an instruction in a loaded image, and what it was verified to hold.
struct OperandRewrite final {
    std::uint32_t operandRva;
    std::uint32_t expected;
    std::uint32_t replacement;
};

enum class RewriteRefusal : std::uint8_t { unexpectedCode, notWritable };

// A set of rewrites in force; destroying it puts the original code back. The thread that applies or
// destroys it must be the one that runs the rewritten code.
class ImageRewrite final {
  public:
    // All or nothing: every operand is checked before the first write.
    [[nodiscard]] static auto apply(std::byte *image, std::vector<OperandRewrite> rewrites) noexcept
        -> std::expected<ImageRewrite, RewriteRefusal>;

    ImageRewrite(const ImageRewrite &) = delete;
    auto operator=(const ImageRewrite &) -> ImageRewrite & = delete;
    ImageRewrite(ImageRewrite &&other) noexcept;
    auto operator=(ImageRewrite &&) -> ImageRewrite & = delete;
    ~ImageRewrite();

  private:
    ImageRewrite(std::byte *image, std::vector<OperandRewrite> rewrites) noexcept
        : image{image}, rewrites{std::move(rewrites)} {}

    std::byte *image;
    std::vector<OperandRewrite> rewrites;
    std::size_t written{};
};

} // namespace extendedPrecache::platform::windows
