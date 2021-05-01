//
// FILE : A kernel object closeable with CloseHandle()
//

#pragma once

#include "detail/winapi.h"

#include "detail/win_macros_begin.inl"

namespace koru
{
template <bool, bool, std::size_t>
class context;
namespace detail
{
class file
{
    template <bool, bool, std::size_t>
    friend class context;

    struct location {
        uint64_t offset;
        HANDLE handle;
#pragma warning(suppress : 4820) /* padding added after data member */
    };

    constexpr file(HANDLE handle) noexcept : native_handle(handle) {}

  public:
    KORU_defctor(file, = delete;);
    ~file()
    {
        [[maybe_unused]] const auto res = CloseHandle(native_handle);
        KORU_assert(res != 0);
    }

    /// @brief A convenience function to facilitate specifying file-location info in read/write operations.
    /// @param offset A byte offset into *this.
    /// @return The coupling of the file handle and given byte offset.
    [[nodiscard]] constexpr location at(uint64_t offset) const noexcept
    {
        return {offset, native_handle};
    }

    /// @brief This is the WinAPI handle representing the file.
    const HANDLE native_handle;
};
} // namespace detail

enum class access : detail::DWORD {
    read       = GENERIC_READ,
    write      = GENERIC_WRITE,
    read_write = GENERIC_READ | GENERIC_WRITE
};

} // namespace koru

#include "detail/win_macros_end.inl"