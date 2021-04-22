//
// I/O CONTEXT : WFMO loop awakening coroutines awaiting async I/Os
//

#pragma once

#include "detail/utils.h"
#include "detail/winapi.h"
#include "file.h"
#include "socket.h"
#include <coroutine>
#include <exception>
#include <mutex>
#include <type_traits>

#include "detail/win_macros_begin.inl"

namespace koru
{
namespace detail
{
SOCKET create_socket(const wchar_t *node, const wchar_t *service,
                     const ADDRINFOW &hints);
} // namespace detail
constexpr inline std::size_t max_ios = MAXIMUM_WAIT_OBJECTS;

/// @brief Orchestrates the awaiting of asynchronous I/Os.
/// @tparam AtomicIos Whether it's possible for multiple I/O submissions to happen simultaneously. Required by AsyncIos.
/// @tparam AsyncIos Whether it's possible for an I/O to be submitted while WaitForMultipleObjects is ongoing.
/// @tparam MaxIos The maximum simultaneously awaited-on I/Os; can not exceed (async_ios ? max_ios - 1 : max_ios).
template <bool AtomicIos = false, bool AsyncIos = false,
          std::size_t MaxIos =
              static_cast<std::size_t>(AsyncIos ? max_ios - 1 : max_ios)>
class context
{
    static_assert(!AsyncIos || AtomicIos, "AtomicIos is required by AsyncIos");
    static_assert(MaxIos <= (AsyncIos ? max_ios - 1 : max_ios),
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
        KORU_inline file_task(context &c, OpT op, detail::HANDLE hfile,
                              uint64_t offset, BufT buf, detail::DWORD nbytes)
            : last_{c.last_}
        {
            ol_.Offset     = static_cast<uint32_t>(offset);
            ol_.OffsetHigh = static_cast<uint32_t>(offset >> 32);
            ol_.hEvent =
                detail::or_(c.evs_[c.last_.sz], KORU_fref(detail::CreateEventW),
                            nullptr, false, false, nullptr);

            if (op(hfile, buf, nbytes, nullptr, &ol_)) {
                // I/O completed synchronously (e.g., cache hit)
                last_.p = nullptr;
                if constexpr (AtomicIos)
                    last_.unlock();
            } else if (GetLastError() != ERROR_IO_PENDING) {
                // Error occurred
                detail::throw_last_winapi_error();
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
        detail::OVERLAPPED ol_;
        struct ptr {
            constexpr KORU_inline ptr(const auto &) {}
            KORU_defctor(ptr, = delete;);
            coro_ptr *p;
        };
        struct ptr_and_lock : detail::lock<false> {
            constexpr KORU_inline ptr_and_lock(auto &x)
                : detail::lock<false>{x.srwl}
            {
            }
            KORU_defctor(ptr_and_lock, = delete;);
            coro_ptr *p;
        };
        std::conditional_t<AtomicIos, ptr_and_lock, ptr> last_;
    };

  public:
    [[nodiscard]] KORU_defctor(context, {
        if (detail::WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
            detail::throw_last_wsa_error();
    });

    ~context()
    {
        WSACleanup();
        for (int sz = 0; evs_[sz]; ++sz)
            CloseHandle(evs_[sz]);
    }

    /// @brief Creates a socket that can be operated on by *this.
    /// @param node String denoting a host name or numeric address.
    /// @param service String denoting a service name or port number.
    /// @param ii The desired protocol family and socket type.
    /// @return An object that represents the created socket.
    [[nodiscard]] KORU_inline socket create(const wchar_t *node,
                                            const wchar_t *service,
                                            detail::inet_info ii = tcp)
    {
        const detail::ADDRINFOW hints{.ai_family   = ii.family,
                                      .ai_socktype = ii.socktype,
                                      .ai_protocol = ii.protocol};
        return {detail::create_socket(node, service, hints)};
    }

    /// @brief Opens a file that can be operated on by *this.
    /// @param fname WinAPI-conformant path specifier denoting a file.
    /// @param acs Kind of operations allowed on the file.
    /// @return An object that represents the opened file.
    [[nodiscard]] file open(const wchar_t *fname, access acs = access::read)
    {
        const auto handle = detail::CreateFileW(
            fname, static_cast<detail::DWORD>(acs),
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            static_cast<detail::DWORD>(acs == access::read ? OPEN_EXISTING
                                                           : OPEN_ALWAYS),
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            detail::throw_last_winapi_error();
        return {handle};
    }

    /// @brief Initiates the read of file that completes either synchronously or asynchronously.
    /// @param l A location on a file opened by *this in a call to the member function open(). The file must have read access.
    /// @param buffer A pointer denoting the recipient buffer.
    /// @param nbytes The maximum number of bytes to read.
    /// @return Task object representing the file operation; must be awaited on immediately.
    [[nodiscard]] KORU_inline file_task read(file::location l, void *buffer,
                                             uint32_t nbytes)
    {
        // The call to ReadFile() has to be 'memoized' like this because the
        // OVERLAPPED structure supplied to the call has to persist in
        // memory throughout the potentionally asynchronously completing
        // I/O.
        return {*this, KORU_fref(ReadFile), l.handle, l.offset, buffer, nbytes};
    }

    /// @brief Initiates the write of file that completes either synchronously or asynchronously.
    /// @param l A location on a file opened by *this in a call to the member function open(). The file must have write access.
    /// @param buffer A pointer denoting the source buffer.
    /// @param nbytes The maximum number of bytes to write.
    /// @return Task object representing the file operation; must be awaited on immediately.
    [[nodiscard]] KORU_inline file_task write(file::location l,
                                              const void *buffer,
                                              uint32_t nbytes)
    {
        // The call to WriteFile() has to be 'memoized' like this because
        // the OVERLAPPED structure supplied to the call has to persist in
        // memory throughout the potentionally asynchronously completing
        // I/O.
        return {*this, KORU_fref(WriteFile), l.handle, l.offset, buffer,
                nbytes};
    }

  private:
    template <bool Shared>
    KORU_inline auto lock_or_empty() noexcept
    {
        if constexpr (AtomicIos)
            return detail::lock<Shared>{last_.srwl};
        else
            return detail::empty{};
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
                WaitForMultipleObjects(static_cast<detail::DWORD>(sz), evs_,
                                       false, INFINITE) -
                WAIT_OBJECT_0);
            if (h_idx > static_cast<unsigned>(sz)) {
                detail::throw_last_winapi_error();
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

  private:
    detail::HANDLE evs_[nmax]{
        detail::if_<!AsyncIos>(nullptr, KORU_fref(detail::CreateEventW),
                               nullptr, false, false, nullptr)};

    // These are for Natvis to be able to display event-coro pairings.
    static constexpr std::size_t off = sizeof(context::evs_);
    struct item;

    coro_ptr coros_[nmax];

    detail::WSADATA wsadata;

    struct size_and_lock {
        detail::SRWLOCK srwl;
        int sz = init_sz;
        KORU_inline KORU_defctor(
            size_and_lock, noexcept { detail::InitializeSRWLock(&srwl); });
#pragma warning(suppress : 4820) /* padding added after data member */
    };
    struct size {
        int sz = init_sz;
    };
    std::conditional_t<AtomicIos, size_and_lock, size> last_;
#pragma warning(suppress : 4820) /* padding added after data member */
};
} // namespace koru

#include "detail/win_macros_end.inl"