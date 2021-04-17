#pragma once

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#define _WIN32_WINNT 0x0600 /* minimum for SRWLs */
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#pragma warning(push, 3)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma warning(pop)

#include <coroutine>
#include <exception>
#include <mutex>
#include <type_traits>

namespace koru
{

enum class access : DWORD {
    read       = GENERIC_READ,
    write      = GENERIC_WRITE,
    read_write = GENERIC_READ | GENERIC_WRITE
};

namespace detail
{
#ifndef KORU_DEBUG
#ifdef NDEBUG
#define KORU_DEBUG 0
#else
#define KORU_DEBUG 1
#endif
#endif

#if KORU_DEBUG
#define KORU_dbg(...) __VA_ARGS__
#define KORU_ndbg(...)
#else
#define KORU_dbg(...)
#define KORU_ndbg(...) __VA_ARGS__
#endif

#if KORU_DEBUG
#define KORU_assert(Expr)                                                      \
    [](bool e) {                                                               \
        if (std::is_constant_evaluated())                                      \
            if (!e)                                                            \
                throw std::runtime_error{#Expr " != true"};                    \
            else if (!e) {                                                     \
                fprintf(stderr, "%s\n", #Expr " != true");                     \
                std::terminate();                                              \
            }                                                                  \
    }(static_cast<bool>(Expr))
#else
#define KORU_assert(Expr) __assume(Expr)
#endif

#define KORU_concat(A, B) KORU_concat_exp(A, B)
#define KORU_concat_exp(A, B) A##B

// Used when it is an error in the design for the inlining not to take place.
#define KORU_inline inline __forceinline

#define KORU_fref_args KORU_concat(FrefArgs, __LINE__)
#define KORU_fref(F)                                                           \
    []<class... KORU_fref_args>(KORU_fref_args && ...args) noexcept(           \
        noexcept(F(static_cast<KORU_fref_args &&>(args)...)))                  \
        ->decltype(                                                            \
            F(static_cast<KORU_fref_args &&>(args)...)) requires requires      \
    {                                                                          \
        F(static_cast<KORU_fref_args &&>(args)...);                            \
    }                                                                          \
    {                                                                          \
        return F(static_cast<KORU_fref_args &&>(args)...);                     \
    }

[[noreturn]] void throw_last_winapi_error();

template <class T, class F, class... Args>
constexpr KORU_inline T &or_(T &val, F &&f, Args &&...args) noexcept(
    noexcept(static_cast<F &&>(f)(static_cast<Args>(args)...)))
{
    if (!val)
        val = static_cast<F &&>(f)(static_cast<Args>(args)...);
    return val;
};

template <bool Test, class T, class F, class... Args>
constexpr KORU_inline auto
if_([[maybe_unused]] T &&x, F &&f, Args &&...args) noexcept(
    noexcept(static_cast<F &&>(f)(static_cast<Args>(args)...)))
    -> std::conditional_t<
        Test, T, decltype(static_cast<F &&>(f)(static_cast<Args>(args)...))>
{
    if constexpr (Test)
        return static_cast<T &&>(x);
    else
        return static_cast<F &&>(f)(static_cast<Args>(args)...);
};

struct empty {
};

template <bool Shared>
class lock
{
  public:
    lock(const lock &) = delete;
    lock(lock &&)      = delete;
    lock &operator=(const lock &) = delete;
    lock &operator=(lock &&) = delete;
    KORU_inline lock(SRWLOCK &srwl) noexcept : srwl_{&srwl}
    {
        if constexpr (Shared)
            AcquireSRWLockShared(&srwl);
        else
            AcquireSRWLockExclusive(&srwl);
    }
    ~lock()
    {
        if (srwl_)
            unlock();
    }
    KORU_inline void unlock() noexcept
    {
        KORU_assert(srwl_);
        if constexpr (Shared)
            ReleaseSRWLockShared(srwl_);
        else
            ReleaseSRWLockExclusive(srwl_);
        srwl_ = nullptr;
    }

  private:
    SRWLOCK *srwl_;
};

//
// SYNCHRONOUS TASK : Coroutine assuming coöperative multitasking
//

template <class T, class Task>
class sync_pointer
{
    template <class>
    friend class sync_task;

    using ex_ptr = std::exception_ptr;
    enum class status : unsigned char { noinit, value, error };

    template <class>
    struct storage {
        template <class>
        friend class sync_task;
        friend class sync_pointer;
        /// @brief Forms a reference to the coroutine result. Rethrows any stored exception.
        /// @return A reference to the object in storage.
        KORU_inline T &get()
        {
            KORU_assert(s == status::value || s == status::error);
            if (s == status::error) [[unlikely]]
                std::rethrow_exception(
                    static_cast<ex_ptr &&>(*std::bit_cast<ex_ptr *>(&buf)));
            // This differs from the move semantics of std::future::get.
            return *std::bit_cast<T *>(&buf);
        }
        ~storage()
        {
            if (s == status::value) [[likely]]
                std::bit_cast<T *>(&buf)->~T();
            else [[unlikely]] if (s == status::error)
                std::bit_cast<ex_ptr *>(&buf)->~ex_ptr();
        }
        std::aligned_union_t<1, ex_ptr, T> buf;
        status s = status::noinit;
#pragma warning(suppress : 4820) /* padding added after data member */
    };

    template <>
    struct storage<void> {
        template <class>
        friend class sync_task;
        friend class sync_pointer;
        /// @brief Rethrows any stored exception.
        KORU_inline void get()
        {
            if (ep) [[unlikely]]
                std::rethrow_exception(static_cast<ex_ptr &&>(ep));
        }
        ex_ptr ep{};
    };

  public:
    constexpr Task get_return_object() noexcept { return {pstore}; }
    constexpr std::suspend_never initial_suspend() const noexcept { return {}; }
    constexpr std::suspend_never final_suspend() const noexcept { return {}; }

    void unhandled_exception() noexcept
    {
        if constexpr (!std::is_void_v<T>) {
            new (&pstore->buf) std::exception_ptr{std::current_exception()};
            pstore->s = status::error;
        } else
            pstore->ep = std::current_exception();
    }

  private:
    storage<T> *pstore;
};

/// @brief Models a synchronously performed task (e.g., no race conditions can occur in the accessing of this object).
/// @tparam T The type of object returned from the coroutine.
template <class T>
class sync_task : public sync_pointer<T, sync_task<T>>::template storage<T>
{
    using base = sync_pointer<T, sync_task<T>>;
    friend class base;

    [[nodiscard]] constexpr KORU_inline
    sync_task(base::template storage<T> *&pref) noexcept
    {
        pref = this;
    }

  public:
    sync_task()                  = delete;
    sync_task(const sync_task &) = delete;
    sync_task(sync_task &&)      = delete;
    sync_task &operator=(const sync_task &) = delete;
    sync_task &operator=(sync_task &&) = delete;

    template <class>
    struct promise : base {
        constexpr KORU_inline promise() noexcept {}
        promise(const promise &) = delete;
        promise(promise &&)      = delete;
        promise &operator=(const promise &) = delete;
        promise &operator=(promise &&) = delete;
        constexpr void return_value(T &&x) noexcept(
            std::is_nothrow_move_constructible_v<
                T>) requires std::is_move_constructible_v<T>
        {
            new (&base::pstore->buf) T{static_cast<T &&>(x)};
            base::pstore->s = base::status::value;
        }

        constexpr void return_value(const T &x) noexcept(
            std::is_nothrow_copy_constructible_v<
                T>) requires std::is_copy_constructible_v<T>
        {
            new (&base::pstore->buf) T{x};
            base::pstore->s = base::status::value;
        }
    };

    template <>
    struct promise<void> : base {
        constexpr KORU_inline promise() noexcept {}
        promise(const promise &) = delete;
        promise(promise &&)      = delete;
        promise &operator=(const promise &) = delete;
        promise &operator=(promise &&) = delete;
        constexpr void return_void() noexcept {}
    };

    using promise_type = promise<T>;
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

    // Make Natvis show the original coroutine function name and signature and
    // the current suspension point (added in VS19 version 16.10 Preview 2).
    using coro_ptr =
        std::conditional_t<KORU_DEBUG, std::coroutine_handle<>, void *>;

    class file
    {
        friend class context;

        struct location {
            uint64_t offset;
            HANDLE handle;
#pragma warning(suppress : 4820) /* padding added after data member */
        };

        constexpr file(HANDLE handle) noexcept : native_handle(handle) {}

      public:
        file(const file &) = delete;
        file(file &&)      = delete;
        file &operator=(const file &) = delete;
        file &operator=(file &&) = delete;
        ~file() { CloseHandle(native_handle); }

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
            ol_.hEvent =
                detail::or_(c.evs_[c.last_.sz], KORU_fref(CreateEventW),
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
        file_task(const file_task &) = delete;
        file_task(file_task &&)      = delete;
        file_task &operator=(const file_task &) = delete;
        file_task &operator=(file_task &&) = delete;

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
            coro_ptr *p;
        };
        struct ptr_and_lock : lock<false> {
            constexpr KORU_inline ptr_and_lock(auto &x) : lock<false>{x.srwl} {}
            ptr_and_lock(const ptr_and_lock &) = delete;
            ptr_and_lock(ptr_and_lock &&)      = delete;
            ptr_and_lock &operator=(const ptr_and_lock &) = delete;
            ptr_and_lock &operator=(ptr_and_lock &&) = delete;
            coro_ptr *p;
        };
        std::conditional_t<AtomicIos, ptr_and_lock, ptr> last_;
    };

  public:
    [[nodiscard]] context()  = default;
    context(const context &) = delete;
    context(context &&)      = delete;
    context &operator=(const context &) = delete;
    context &operator=(context &&) = delete;

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
            detail::throw_last_winapi_error();
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

        while (sz) {
            const auto h_idx = static_cast<unsigned>(
                WaitForMultipleObjects(static_cast<DWORD>(sz), evs_, false,
                                       INFINITE) -
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
    static constexpr int init_sz     = AsyncIos ? 1 : 0;
    struct item;

    struct size_and_lock {
        SRWLOCK srwl;
        int sz = init_sz;
        KORU_inline size_and_lock() noexcept { InitializeSRWLock(&srwl); }
        size_and_lock(const size_and_lock &) = delete;
        size_and_lock(size_and_lock &&)      = delete;
        size_and_lock &operator=(const size_and_lock &) = delete;
        size_and_lock &operator=(size_and_lock &&) = delete;
#pragma warning(suppress : 4820) /* padding added after data member */
    };
    struct size {
        int sz = init_sz;
    };
    std::conditional_t<AtomicIos, size_and_lock, size> last_;
};

} // namespace detail

using detail::context;
using detail::sync_task;

} // namespace koru