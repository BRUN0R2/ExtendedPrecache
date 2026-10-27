#pragma once

#include <extendedPrecache/data/types.hpp>

#include <bit>
#include <cmath>
#include <concepts>
#include <functional>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace extendedPrecache::data::detail {
static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

class DecodeFailure final : public std::exception {
  public:
    explicit DecodeFailure(Failure input) noexcept : failure{input} {}
    [[nodiscard]] auto what() const noexcept -> const char * override {
        return "Data contract violation";
    }
    Failure failure;
};
// A checked cursor unwinds only to the codec boundary; no partial result escapes.
[[noreturn]] inline void fail(ErrorCode code = ErrorCode::malformed, std::uint64_t offset = 0,
                              std::uint32_t field = noSubject) {
    throw DecodeFailure{Failure{code, noSubject, field, offset}};
}
inline void require(bool condition, ErrorCode code = ErrorCode::malformed) {
    if (!condition) {
        fail(code);
    }
}
template <typename Function>
auto protect(Function &&function) noexcept -> Result<std::invoke_result_t<Function>> {
    try {
        if constexpr (std::is_void_v<std::invoke_result_t<Function>>) {
            std::invoke(std::forward<Function>(function));
            return {};
        } else {
            return std::invoke(std::forward<Function>(function));
        }
    } catch (const DecodeFailure &error) {
        return std::unexpected{error.failure};
    } catch (const std::bad_alloc &) {
        return std::unexpected{Failure{ErrorCode::storageUnavailable}};
    } catch (const std::length_error &) {
        return std::unexpected{Failure{ErrorCode::byteBudget}};
    } catch (const std::bad_variant_access &) {
        return std::unexpected{Failure{ErrorCode::invalidSchema}};
    }
}
template <typename T> auto get(Result<T> result) -> T {
    if (!result) {
        throw DecodeFailure{result.error()};
    }
    if constexpr (!std::is_void_v<T>) {
        return std::move(*result);
    }
}
inline auto add(std::uint64_t left, std::uint64_t right) -> std::uint64_t {
    require(right <= std::numeric_limits<std::uint64_t>::max() - left, ErrorCode::byteBudget);
    return left + right;
}
inline auto multiply(std::uint64_t left, std::uint64_t right) -> std::uint64_t {
    require(left == 0 || right <= std::numeric_limits<std::uint64_t>::max() / left,
            ErrorCode::byteBudget);
    return left * right;
}
inline void within(std::uint64_t amount, std::uint64_t limit) {
    require(amount <= limit, ErrorCode::byteBudget);
}
inline auto narrow(std::size_t value) -> std::uint32_t {
    within(value, std::numeric_limits<std::uint32_t>::max());
    return static_cast<std::uint32_t>(value);
}
inline auto small(std::size_t value) -> std::uint16_t {
    within(value, std::numeric_limits<std::uint16_t>::max());
    return static_cast<std::uint16_t>(value);
}
inline auto hash(std::span<const std::byte> input, const DigestProvider &provider) -> Digest {
    return get(sha256(input, provider));
}
inline void validFloat(float value) {
    require(std::isfinite(value), ErrorCode::malformed);
}
inline auto canonicalFloat(float value) -> float {
    validFloat(value);
    return value == 0 ? 0.0F : value;
}
inline void canonicalizeValues(ValueSet &values) {
    for (auto &field : values) {
        for (auto &atom : field.elements) {
            if (auto *scalar = std::get_if<float>(&atom)) {
                *scalar = canonicalFloat(*scalar);
            }
            if (auto *vector = std::get_if<Vec3>(&atom)) {
                for (auto &component : *vector) {
                    component = canonicalFloat(component);
                }
            }
        }
    }
}
inline auto ownedFieldBytes(const FieldValue &value) -> std::uint64_t {
    auto bytes = multiply(value.elements.size(), sizeof(Atom));
    for (const auto &atom : value.elements) {
        if (const auto *text = std::get_if<ByteString>(&atom)) {
            bytes = add(bytes, text->bytes.size());
        }
    }
    return bytes;
}
inline auto ownedValueBytes(const ValueSet &values) -> std::uint64_t {
    auto bytes = multiply(values.size(), sizeof(FieldValue));
    for (const auto &field : values) {
        bytes = add(bytes, ownedFieldBytes(field));
    }
    return bytes;
}
inline auto identifier(std::string_view value, std::size_t maximum = maxNameBytes) -> bool {
    const auto letter = [](char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
    };
    if (value.empty() || value.size() > maximum || !letter(value.front())) {
        return false;
    }
    return std::ranges::all_of(value,
                               [&](char ch) { return letter(ch) || (ch >= '0' && ch <= '9'); });
}

class Reader final {
  public:
    explicit Reader(std::span<const std::byte> input) : source{input} {}
    [[nodiscard]] auto remaining() const noexcept -> std::size_t {
        return source.size() - position;
    }
    [[nodiscard]] auto offset() const noexcept -> std::size_t {
        return position;
    }
    auto take(std::size_t count) -> std::span<const std::byte> {
        if (count > remaining()) {
            fail(ErrorCode::malformed, position);
        }
        const auto part = source.subspan(position, count);
        position += count;
        return part;
    }
    template <std::unsigned_integral T> auto integer() -> T {
        const auto part = take(sizeof(T));
        std::uint64_t value{};
        for (std::size_t index = 0; index < sizeof(T); ++index) {
            value |= std::to_integer<std::uint64_t>(part[index]) << (index * 8U);
        }
        return static_cast<T>(value);
    }
    template <typename T> auto enumeration() -> T {
        return static_cast<T>(integer<std::underlying_type_t<T>>());
    }
    template <typename T> auto identity() -> T {
        T result;
        std::ranges::copy(take(result.bytes.size()), result.bytes.begin());
        return result;
    }
    auto f32() -> float {
        const auto bits = integer<std::uint32_t>();
        const auto value = std::bit_cast<float>(bits);
        validFloat(value);
        require(bits != 0x80000000U);
        return value;
    }
    void zero(std::size_t count) {
        require(std::ranges::all_of(take(count), [](std::byte ch) { return ch == std::byte{}; }));
    }
    auto text(std::size_t count, std::size_t maximum) -> std::string {
        within(count, maximum);
        const auto part = take(count);
        return {reinterpret_cast<const char *>(part.data()), part.size()};
    }
    void count(std::size_t amount, std::size_t minimumBytes) const {
        require(amount <= remaining() / minimumBytes);
    }
    void end() const {
        require(remaining() == 0);
    }

  private:
    std::span<const std::byte> source;
    std::size_t position{};
};

class Writer final {
  public:
    explicit Writer(std::uint64_t maximum) : limit{maximum} {}
    template <std::unsigned_integral T> void integer(T value) {
        room(sizeof(T));
        for (std::size_t index = 0; index < sizeof(T); ++index) {
            output.push_back(
                static_cast<std::byte>((static_cast<std::uint64_t>(value) >> (index * 8U)) & 255U));
        }
    }
    template <typename T> void enumeration(T value) {
        integer(static_cast<std::underlying_type_t<T>>(value));
    }
    void bytes(std::span<const std::byte> value) {
        room(value.size());
        output.insert(output.end(), value.begin(), value.end());
    }
    void text(std::string_view value) {
        bytes(std::as_bytes(std::span{value.data(), value.size()}));
    }
    void zero(std::size_t count) {
        room(count);
        output.insert(output.end(), count, std::byte{});
    }
    void f32(float value) {
        validFloat(value);
        integer(std::bit_cast<std::uint32_t>(value == 0 ? 0.0F : value));
    }
    [[nodiscard]] auto view() const noexcept -> std::span<const std::byte> {
        return output;
    }
    [[nodiscard]] auto finish() && -> Bytes {
        return std::move(output);
    }

  private:
    void room(std::size_t count) {
        const auto required = add(output.size(), count);
        within(required, limit);
        within(required, output.max_size());
        if (required > output.capacity()) {
            const auto capacity =
                std::min(limit, std::max(required, multiply(output.capacity(), 2)));
            output.reserve(static_cast<std::size_t>(capacity));
            within(output.capacity(), limit);
        }
    }
    Bytes output;
    std::uint64_t limit;
};

inline void inputBudget(std::span<const std::byte> bytes, RecordBudget budget) {
    within(bytes.size(), budget.messageBytes.value);
    within(bytes.size(), budget.ownedBytes.value);
}
class AllocationBudget final {
  public:
    explicit AllocationBudget(MetadataBytes limit) : remaining{limit.value} {}
    void charge(std::uint64_t bytes) {
        within(bytes, remaining);
        remaining -= bytes;
    }

  private:
    std::uint64_t remaining;
};
} // namespace extendedPrecache::data::detail
