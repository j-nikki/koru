#pragma once

#include "detail/utils.h"
#include "detail/winapi.h"
#include <coroutine>
#include <exception>
#include <mutex>
#include <type_traits>

#include "detail/win_macros_begin.inl"

namespace koru
{
namespace detail
{

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

//
// I/O CONTEXT : WFMO loop awakening coroutines awaiting async I/Os
//

/// @brief Orchestrates the awaiting of asynchronous I/Os.
/// @tparam AtomicIos Whether it's possible for multiple I/O submissions to happen simultaneously. Required by AsyncIos.
/// @tparam AsyncIos Whether it's possible for an I/O to be submitted while WaitForMultipleObjects is ongoing.
/// @tparam MaxIos The maximum simultaneously awaited-on I/Os; can not exceed (async_ios ? MAXIMUM_WAIT_OBJECTS - 1 : MAXIMUM_WAIT_OBJECTS).
template <bool AtomicIos = false, bool AsyncIos = false,
          std::size_t MaxIos = static_cast<std::size_t>(
              AsyncIos ? MAXIMUM_WAIT_OBJECTS - 1 : MAXIMUM_WAIT_OBJECTS)>
class context
{
    static_assert(!AsyncIos || AtomicIos, "AtomicIos is required by AsyncIos");
    static_assert(MaxIos <= (AsyncIos ? MAXIMUM_WAIT_OBJECTS - 1
                                      : MAXIMUM_WAIT_OBJECTS),
                  "MaxIos is too big");

    static constexpr std::size_t nmax = AsyncIos ? MaxIos + 1 : MaxIos;
    static constexpr int init_sz      = AsyncIos ? 1 : 0;

    // Make Natvis show the original coroutine function name and signature and
    // the current suspension point (added in VS19 version 16.10 Preview 2).
    using coro_ptr =
        std::conditional_t<KORU_DEBUG, std::coroutine_handle<>, void *>;

    class file_task
    {
        friend class context;

        template <class OpT, class BufT>
        KORU_inline file_task(context &c, OpT op, HANDLE hfile, uint64_t offset,
                              BufT buf, DWORD nbytes)
            : last_{c.last_}
        {
            ol_.Offset     = static_cast<uint32_t>(offset);
            ol_.OffsetHigh = static_cast<uint32_t>(offset >> 32);
            ol_.hEvent     = or_(c.evs_[c.last_.sz], KORU_fref(CreateEventW),
                             nullptr, false, false, nullptr);

            if (op(hfile, buf, nbytes, nullptr, &ol_)) {
                // I/O completed synchronously (e.g., cache hit)
                last_.p = nullptr;
                if constexpr (AtomicIos)
                    last_.unlock();
            } else if (GetLastError() != ERROR_IO_PENDING) {
                // Error occurred
                throw_last_winapi_error();
            } else {
                // Async I/O initiated successfully
                last_.p = &c.coros_[c.last_.sz++];
                if constexpr (AsyncIos)
                    SetEvent(c.evs_[0]);
            }
        }

      public:
        KORU_defctor(file_task, = delete;);

        bool await_ready() const noexcept { return !last_.p; }
        auto await_resume() const noexcept
        {
            return std::bit_cast<std::size_t>(ol_.InternalHigh);
        }
        void await_suspend(std::coroutine_handle<> h)
        {
            *last_.p = h KORU_ndbg(.address());
            if constexpr (AtomicIos)
                last_.unlock();
        }

      private:
        OVERLAPPED ol_;
        struct ptr {
            constexpr KORU_inline ptr(const auto &) {}
            KORU_defctor(ptr, = delete;);
            coro_ptr *p;
        };
        struct ptr_and_lock : lock<false> {
            constexpr KORU_inline ptr_and_lock(auto &x) : lock<false>{x.srwl} {}
            KORU_defctor(ptr_and_lock, = delete;);
            coro_ptr *p;
        };
        std::conditional_t<AtomicIos, ptr_and_lock, ptr> last_;
    };

  public:
    [[nodiscard]] KORU_defctor(context, = default;);

    /// @brief Opens a file that can be operated on by *this.
    /// @param fname WinAPI-conformant path specifier denoting a file.
    /// @param acs Kind of operations allowed on the file.
    /// @return An object that represents the opened file.
    [[nodiscard]] file open(const wchar_t *fname, access acs = access::read)
    {
        const auto handle = CreateFileW(
            fname, static_cast<DWORD>(acs),
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            static_cast<DWORD>(acs == access::read ? OPEN_EXISTING
                                                   : OPEN_ALWAYS),
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw_last_winapi_error();
        return {handle};
    }

    /// @brief Initiates the read of file that completes either synchronously or asynchronously.
    /// @param hfile A file opened by *this in a call to the member function open(). Must have read access.
    /// @param offset A byte offset at which to start reading.
    /// @param buffer A pointer denoting the recipient buffer.
    /// @param nbytes The maximum number of bytes to read.
    /// @return Task object representing the file operation; must be awaited on immediately.
    [[nodiscard]] KORU_inline file_task read(HANDLE hfile, uint64_t offset,
                                             LPVOID buffer, DWORD nbytes)
    {
        // The call to ReadFile() has to be 'memoized' like this because the
        // OVERLAPPED structure supplied to the call has to persist in
        // memory throughout the potentionally asynchronously completing
        // I/O.
        return {*this, KORU_fref(ReadFile), hfile, offset, buffer, nbytes};
    }

    /// @brief Initiates the read of file that completes either synchronously or asynchronously.
    /// @param l A location on a file opened by *this in a call to the member function open(). The file must have read access.
    /// @param buffer A pointer denoting the recipient buffer.
    /// @param nbytes The maximum number of bytes to read.
    /// @return Task object representing the file operation; must be awaited on immediately.
    [[nodiscard]] KORU_inline auto read(file::location l, LPVOID buffer,
                                        DWORD nbytes)
    {
        return read(l.handle, l.offset, buffer, nbytes);
    }

    /// @brief Initiates the write of file that completes either synchronously or asynchronously.
    /// @param hfile A file opened by *this in a call to the member function open(). Must have write access.
    /// @param offset A byte offset at which to start writing.
    /// @param buffer A pointer denoting the source buffer.
    /// @param nbytes The maximum number of bytes to write.
    /// @return Task object representing the file operation; must be awaited on immediately.
    [[nodiscard]] KORU_inline file_task write(HANDLE hfile, uint64_t offset,
                                              LPCVOID buffer, DWORD nbytes)
    {
        // The call to WriteFile() has to be 'memoized' like this because
        // the OVERLAPPED structure supplied to the call has to persist in
        // memory throughout the potentionally asynchronously completing
        // I/O.
        return {*this, KORU_fref(WriteFile), hfile, offset, buffer, nbytes};
    }

    /// @brief Initiates the write of file that completes either synchronously or asynchronously.
    /// @param l A location on a file opened by *this in a call to the member function open(). The file must have write access.
    /// @param buffer A pointer denoting the source buffer.
    /// @param nbytes The maximum number of bytes to write.
    /// @return Task object representing the file operation; must be awaited on immediately.
    [[nodiscard]] KORU_inline auto write(file::location l, LPCVOID buffer,
                                         DWORD nbytes)
    {
        return write(l.handle, l.offset, buffer, nbytes);
    }

  private:
    template <bool Shared>
    KORU_inline auto lock_or_empty() noexcept
    {
        if constexpr (AtomicIos)
            return lock<Shared>{last_.srwl};
        else
            return empty{};
    }

  public:
    /// @brief Responds to tracked I/O completions by resuming the corresponding awaiting coroutine. Exits after running out of work.
    void run()
    {
        auto sz = [&] {
            const auto loe = lock_or_empty<true>();
            return last_.sz;
        }();

        while (sz != init_sz) {
            const auto h_idx = static_cast<unsigned>(
                WaitForMultipleObjects(static_cast<DWORD>(sz), evs_, false,
                                       INFINITE) -
                WAIT_OBJECT_0);
            if (h_idx > static_cast<unsigned>(sz)) {
                throw_last_winapi_error();
#pragma warning(suppress : 4127) /* conditional expression is constant */
            } else if (!AsyncIos || h_idx != 0) {
                // Dequeue and resume the corresponding coro
                const auto ptr = [&] {
                    const auto loe = lock_or_empty<false>();
                    if constexpr (AsyncIos)
                        sz = --last_.sz;
                    else
                        last_.sz = --sz;
                    std::swap(evs_[h_idx], evs_[sz]);
                    return std::exchange(coros_[h_idx], coros_[sz]);
                }();
                KORU_ndbg(std::coroutine_handle<>::from_address)(ptr).resume();
            }
            const auto loe = lock_or_empty<true>();
            sz             = last_.sz;
        }
    }

    ~context()
    {
        while (last_.sz--)
            CloseHandle(evs_[last_.sz]);
    }

  private:
    HANDLE evs_[nmax]{if_<!AsyncIos>(nullptr, KORU_fref(CreateEventW), nullptr,
                                     false, false, nullptr)};
    coro_ptr coros_[nmax];

    // These are for Natvis to be able to display event-coro pairings.
    static constexpr std::size_t off = sizeof(evs_);
    struct item;

    struct size_and_lock {
        SRWLOCK srwl;
        int sz = init_sz;
        KORU_inline KORU_defctor(
            size_and_lock, noexcept { InitializeSRWLock(&srwl); });
#pragma warning(suppress : 4820) /* padding added after data member */
    };
    struct size {
        int sz = init_sz;
    };
    std::conditional_t<AtomicIos, size_and_lock, size> last_;
#pragma warning(suppress : 4820) /* padding added after data member */
};

} // namespace detail

using detail::access;
using detail::context;
using detail::file;

} // namespace koru

#include "detail/win_macros_end.inl"