#pragma once

#include "detail/winapi.h"

#include "detail/win_macros_begin.inl"

namespace koru
{
namespace detail
{

//
// FILE : A kernel object closeable with CloseHandle()
//

enum class access : DWORD {
    read       = GENERIC_READ,
    write      = GENERIC_WRITE,
    read_write = GENERIC_READ | GENERIC_WRITE
};

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
using detail::access;
using detail::file;
} // namespace koru

#include "detail/win_macros_end.inl"