#pragma once

#include <cstddef>
#include <cstdio>
#include <exception>
#include <iostream>
#include <source_location>
#include <string_view>

namespace extendedPrecache::tests {

template <typename Run> auto runTests(Run run) noexcept -> int {
    try {
        return run();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Test execution failed: %s\n", error.what());
        return 1;
    } catch (...) {
        // Exceptions at the process boundary are test failures, including foreign provider faults.
        std::fputs("Test execution failed with an unknown exception\n", stderr);
        return 1;
    }
}

class TestSuite final {
  public:
    [[nodiscard]] auto check(bool condition, std::string_view description,
                             std::source_location location = std::source_location::current())
        -> bool {
        ++checkCount;
        if (!condition) {
            ++failureCount;
            std::cerr << location.file_name() << ':' << location.line() << ": " << description
                      << '\n';
        }
        return condition;
    }

    [[nodiscard]] auto finish() const -> int {
        std::cout << checkCount << " checks, " << failureCount << " failures\n";
        return failureCount == 0 ? 0 : 1;
    }

  private:
    std::size_t checkCount{0};
    std::size_t failureCount{0};
};

} // namespace extendedPrecache::tests
