#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace extendedPrecache::platform::windows::client {

// Moves the selected hw.dll's descriptors of known models and their CRC records. Pointers to a
// descriptor escape everywhere, and the engine's cache keeps pointers back into it, so they cannot
// move once the engine has made one. Unlike the model table, this lasts for the whole process.
class ClientModelDescriptors final {
  public:
    // Empty when the configuration asks for no more than the engine already has. It may ask for
    // less than has been verified, never for more.
    [[nodiscard]] static auto reserve(std::uint32_t cells)
        -> std::unique_ptr<ClientModelDescriptors>;

    // Call as the engine is mapped, before any of its code runs. This runs under the loader lock,
    // so it only checks and writes memory.
    [[nodiscard]] auto moveInto(std::byte *image) noexcept -> bool;

  private:
    explicit ClientModelDescriptors(std::uint32_t cells);

    std::uint32_t cells;
    std::unique_ptr<std::byte[]> descriptors;
    std::unique_ptr<std::byte[]> crcRecords;
};

} // namespace extendedPrecache::platform::windows::client
