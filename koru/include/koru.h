#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <coroutine>
#include <exception>
#include <mutex>
#include <type_traits>

namespace koru
{
// Koru (from Finglish korutiini, coroutine) is a library that facilitates using
// overlapped (asynchronous) I/O operations on Windows. It uses C++20 coroutines
// to keep the library use-site coherent - no alternative will be provided as
// this project is specifically meant to demonstrate the new of C++20.
//
// In the 2008 book Concurrent Programming on Windows by Joe Duffy, different
// models of notifying an awaiter of an overlapped I/O completion are presented.
// IOCP was presented as the go-to rendezvous mechanism for any "serious" async
// I/O. WaitForMultipleObjects, the function this library uses, has a limit of
// waiting on only up to MAXIMUM_WAIT_OBJECTS handles* and engages no thread
// pools / APCs, latter possibly being a pro in the sense that it models
// coöperative multitasking - no control mechanisms to govern access on shared
// resources, for example, is required.
//
// On the account of aforementioned, it is evident that the approach opted for
// here does not scale, but well suffices in demonstrating yet another use case
// for C++20 coroutines.
//
// *It is possible to wait on more than MAXIMUM_WAIT_OBJECTS handles, say, by
// creating ⌈N/MAXIMUM_WAIT_OBJECTS⌉ threads, where N is the number of handles
// to wait on, and each thread waits on up to MAXIMUM_WAIT_OBJECTS handles.

enum class access : DWORD {
    read       = GENERIC_READ,
    write      = GENERIC_WRITE,
    read_write = GENERIC_READ | GENERIC_WRITE
};

namespace detail
{
#define KORU_concat(A, B) KORU_concat_exp(A, B)
#define KORU_concat_exp(A, B) A##B

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
constexpr KORU_inline auto if_(T &&x, F &&f, Args &&...args) noexcept(
    noexcept(static_cast<F &&>(f)(static_cast<Args>(args)...)))
    -> std::conditional_t<
        Test, T, decltype(static_cast<F &&>(f)(static_cast<Args>(args)...))>
{
    if constexpr (Test)
        return static_cast<T &&>(x);
    else
        return static_cast<F &&>(f)(static_cast<Args>(args)...);
};

template <class T>
concept stores_exceptions = requires(T &x)
{
    x.set_exception(std::current_exception());
};

struct empty {
};

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
    []() {                                                                     \
        if (std::is_constant_evaluated())                                      \
            return [](bool e) {                                                \
                if (!e)                                                        \
                    throw std::runtime_error{#Expr " != true"};                \
            };                                                                 \
        else                                                                   \
            return [](bool e) {                                                \
                if (!e) {                                                      \
                    fprintf(stderr, "%s\n", #Expr " != true");                 \
                    std::terminate();                                          \
                }                                                              \
            };                                                                 \
    }()(Expr)
#else
#define KORU_assert(Expr) __assume(Expr)
#endif

//
// SYNCHRONOUS TASK : Coroutine assuming coöperative multitasking
//

template <class T, class Task>
class sync_pointer
{
    template <class>
    friend class sync_task;

    using ex_ptr = std::exception_ptr;

    template <class>
    struct storage {
        template <class>
        friend class sync_task;
        friend class sync_pointer;

        constexpr storage()
        {
            KORU_dbg(std::fill_n(std::bit_cast<char *>(&buf),
                                 sizeof(std::exception_ptr), 0xddi8));
        }

        /// @brief Forms a reference to the coroutine result. Rethrows any stored exception.
        /// @return A reference to the object in storage.
        KORU_inline T &get()
        {
            if (error)
                std::rethrow_exception(
                    static_cast<ex_ptr &&>(*std::bit_cast<ex_ptr *>(&buf)));
            // This differs from the move semantics of std::future::get.
            return *std::bit_cast<T *>(&buf);
        }
        ~storage()
        {
            if (error)
                std::bit_cast<ex_ptr *>(&buf)->~ex_ptr();
            else
                std::bit_cast<T *>(&buf)->~T();
        }
        std::aligned_union_t<1, ex_ptr, T> buf;
        bool error;
    };

    template <>
    struct storage<void> {
        template <class>
        friend class sync_task;
        friend class sync_pointer;

        constexpr storage()
        {
            KORU_dbg(std::fill_n(std::bit_cast<char *>(&buf),
                                 sizeof(std::exception_ptr), 0xddi8));
        }

        /// @brief Rethrows any stored exception.
        KORU_inline void get()
        {
            if (auto &e = *std::bit_cast<ex_ptr *>(&buf))
                std::rethrow_exception(static_cast<ex_ptr &&>(e));
        }
        ~storage() { std::bit_cast<ex_ptr *>(&buf)->~ex_ptr(); }
        std::aligned_storage_t<sizeof(ex_ptr), alignof(ex_ptr)> buf;
    };

  public:
    constexpr Task get_return_object() noexcept { return {pstore}; }
    constexpr std::suspend_never initial_suspend() const noexcept { return {}; }
    constexpr std::suspend_never final_suspend() const noexcept { return {}; }

    void unhandled_exception() noexcept
    {
        new (&pstore->buf) std::exception_ptr{std::current_exception()};
        if constexpr (!std::is_void_v<T>)
            pstore->error = true;
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

    constexpr KORU_inline sync_task() noexcept {}
    constexpr KORU_inline sync_task(base::template storage<T> *&pref) noexcept
    {
        pref = this;
    }

  public:
    sync_task(sync_task &&)      = delete;
    sync_task(const sync_task &) = delete;

    template <class>
    struct promise : base {
        constexpr void return_value(T &&x) noexcept(
            std::is_nothrow_move_constructible_v<
                T>) requires std::is_move_constructible_v<T>
        {
            KORU_dbg(std::fill_n(std::bit_cast<char *>(&base::pstore->buf),
                                 sizeof(std::exception_ptr), '\0'));
            new (&base::pstore->buf) T{static_cast<T &&>(x)};
            base::pstore->error = false;
        }

        constexpr void return_value(const T &x) noexcept(
            std::is_nothrow_copy_constructible_v<
                T>) requires std::is_copy_constructible_v<T>
        {
            KORU_dbg(std::fill_n(std::bit_cast<char *>(&base::pstore->buf),
                                 sizeof(std::exception_ptr), '\0'));
            new (&base::pstore->buf) T{x};
            base::pstore->error = false;
        }
    };

    template <>
    struct promise<void> : base {
        constexpr void return_void() noexcept
        {
            KORU_dbg(std::fill_n(std::bit_cast<char *>(&base::pstore->buf),
                                 sizeof(std::exception_ptr), '\0'));
            new (&base::pstore->buf) base::ex_ptr{};
        }
    };

    using promise_type = promise<T>;
};

/// @brief Dictates the behavior of koru::context.
struct context_settings {
    // Whether it's possible for an I/O to be submitted while
    // WaitForMultipleObjects is ongoing.
    bool async_ios = false;
    // Whether it's possible for multiple I/O submissions to happen
    // simultaneously. Required by async_ios.
    bool atomic_ios = async_ios;
    // The maximum simultaneously awaited-on I/Os. Can not exceed (async_ios
    // ? MAXIMUM_WAIT_OBJECTS - 1 : MAXIMUM_WAIT_OBJECTS).
    std::size_t max_ios = static_cast<std::size_t>(
        async_ios ? MAXIMUM_WAIT_OBJECTS - 1 : MAXIMUM_WAIT_OBJECTS);
};

//
// I/O CONTEXT : WFMO loop awakening coroutines awaiting async I/Os
//

#ifdef __INTELLISENSE__ /* 20210413 IntelliSense doesn't seem to like NTTP */
template <class = void>
class context
{
    static constexpr context_settings CS = {};
#else
/// @brief Orchestrates the awaiting of asynchronous I/Os.
/// @tparam CS Dictates the behavior of instances.
template <context_settings CS = {}>
class context
{
#endif
    static_assert(!CS.async_ios || CS.atomic_ios,
                  "atomic_ios is required by async_ios");
    static_assert(CS.max_ios <= (CS.async_ios ? MAXIMUM_WAIT_OBJECTS - 1
                                              : MAXIMUM_WAIT_OBJECTS),
                  "max_ios is too big");

    static constexpr std::size_t nmax =
        CS.async_ios ? CS.max_ios + 1 : CS.max_ios;

    // Make Natvis show the original coroutine function name and signature and
    // the current suspension point (added in VS19 version 16.10 Preview 2).
    using coro_ptr =
        std::conditional_t<KORU_DEBUG, std::coroutine_handle<>, void *>;

    class file
    {
        friend class context;

        struct location {
            const HANDLE handle;
            const uint64_t offset;
        };

        constexpr file(HANDLE handle) noexcept : native_handle(handle) {}

      public:
        file(file &&)      = delete;
        file(const file &) = delete;
        ~file() { CloseHandle(native_handle); }

        /// @brief A convenience function to facilitate specifying file-location info in read/write operations.
        /// @param offset A byte offset into *this.
        /// @return The coupling of the file handle and given byte offset.
        [[nodiscard]] constexpr location at(uint64_t offset) const noexcept
        {
            return {native_handle, offset};
        }

        /// @brief This is the WinAPI handle representing the file.
        const HANDLE native_handle;
    };

    class file_task
    {
        friend class context;

        template <class OpT, class BufT>
        constexpr KORU_inline file_task(context &c, OpT op, HANDLE hfile,
                                        uint64_t offset, BufT buf, DWORD nbytes)
        {
            if constexpr (CS.atomic_ios)
                last_.lk = {c.last_.mtx};

            ol_.Pointer = std::bit_cast<PVOID>(offset);
            ol_.hEvent =
                detail::or_(c.evs_[c.last_.sz], KORU_fref(CreateEventW),
                            nullptr, false, false, nullptr);

            if (op(hfile, buf, nbytes, nullptr, &ol_)) {
                // I/O completed synchronously (e.g., cache hit)
                last_.p = nullptr;
                if constexpr (CS.atomic_ios)
                    last_.lk.unlock();
            } else if (GetLastError() != ERROR_IO_PENDING) {
                // Error occurred
                detail::throw_last_winapi_error();
            } else {
                // Async I/O initiated successfully
                last_.p = &c.coros_[c.last_.sz++];
                if constexpr (CS.async_ios)
                    SetEvent(c.evs_[0]);
            }
        }

      public:
        file_task(const file_task &) = delete;
        file_task(file_task &&)      = delete;
        ~file_task() {}

        bool await_ready() const noexcept { return !last_.p; }
        auto await_resume() const noexcept
        {
            return std::bit_cast<std::size_t>(ol_.InternalHigh);
        }
        void await_suspend(std::coroutine_handle<> h)
        {
            *last_.p = h KORU_ndbg(.address());
            if constexpr (CS.atomic_ios)
                last_.lk.unlock();
        }

      private:
        OVERLAPPED ol_;
        struct ptr {
            coro_ptr *p;
        };
        struct ptr_and_lock {
            coro_ptr *p;
            std::unique_lock<std::mutex> lk;
        };
        std::conditional_t<CS.atomic_ios, ptr_and_lock, ptr> last_;
    };

  public:
    /// @brief Opens a file that can be operated on by *this.
    /// @param fname WinAPI-conformant path specifier denoting a file.
    /// @param acs Kind of operations allowed on the file.
    /// @return An object that represents the opened file.
    [[nodiscard]] file open(const wchar_t *fname, access acs = access::read)
    {
        auto handle = CreateFileW(
            fname, static_cast<DWORD>(acs),
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            acs == access::read ? OPEN_EXISTING : OPEN_ALWAYS,
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
    [[nodiscard]] file_task read(HANDLE hfile, uint64_t offset, LPVOID buffer,
                                 DWORD nbytes)
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
    [[nodiscard]] auto read(file::location l, LPVOID buffer, DWORD nbytes)
    {
        return read(l.handle, l.offset, buffer, nbytes);
    }

    /// @brief Initiates the write of file that completes either synchronously or asynchronously.
    /// @param hfile A file opened by *this in a call to the member function open(). Must have write access.
    /// @param offset A byte offset at which to start writing.
    /// @param buffer A pointer denoting the source buffer.
    /// @param nbytes The maximum number of bytes to write.
    /// @return Task object representing the file operation; must be awaited on immediately.
    [[nodiscard]] file_task write(HANDLE hfile, uint64_t offset, LPCVOID buffer,
                                  DWORD nbytes)
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
    [[nodiscard]] auto write(file::location l, LPCVOID buffer, DWORD nbytes)
    {
        return write(l.handle, l.offset, buffer, nbytes);
    }

  private:
    static KORU_inline auto lock_or_empty() noexcept
    {
        if constexpr (CS.atomic_ios)
            return std::scoped_lock{last_.mtx};
        else
            return empty{};
    }

  public:
    /// @brief Responds to tracked I/O completions by resuming the corresponding awaiting coroutine. Exits after running out of work.
    void run()
    {
        auto sz = [&] {
            const auto loe = lock_or_empty();
            return last_.sz;
        }();

        while (sz) {
            const auto h_idx = static_cast<unsigned>(
                WaitForMultipleObjects(static_cast<DWORD>(sz), evs_, false,
                                       INFINITE) -
                WAIT_OBJECT_0);
            if (h_idx > static_cast<unsigned>(sz)) {
                detail::throw_last_winapi_error();
            } else if (!CS.async_ios || h_idx != 0) {
                // Dequeue and resume the corresponding coro
                const auto ptr = [&] {
                    const auto loe = lock_or_empty();
                    if constexpr (CS.async_ios)
                        sz = --last_.sz;
                    else
                        last_.sz = --sz;
                    std::swap(evs_[h_idx], evs_[sz]);
                    return std::exchange(coros_[h_idx], coros_[sz]);
                }();
#if KORU_DEBUG
                ptr();
#else
                std::coroutine_handle<>::from_address(ptr).resume();
#endif
            }
            const auto loe = lock_or_empty();
            sz             = last_.sz;
        }
    }

    ~context()
    {
        while (last_.sz--)
            CloseHandle(evs_[last_.sz]);
    }

  private:
    HANDLE evs_[nmax]{if_<!CS.async_ios>(nullptr, KORU_fref(CreateEventW),
                                         nullptr, false, false, nullptr)};
    coro_ptr coros_[nmax];

    // These are for Natvis to be able to display event-coro pairings.
    static constexpr std::size_t off = sizeof(evs_);
    static constexpr int init_sz     = CS.async_ios ? 1 : 0;
    struct item;

    // "Tabkeeping". sz is volatile because run() would have UB otherwise.
    struct size_and_mutex {
        std::mutex mtx;
        volatile int sz = init_sz;
    };
    struct size {
        volatile int sz = init_sz;
    };
    std::conditional_t<CS.atomic_ios, size_and_mutex, size> last_;
};

} // namespace detail

using detail::context;
using detail::context_settings;
using detail::sync_task;

} // namespace koru