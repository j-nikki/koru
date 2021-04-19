#pragma once

#include "detail/utils.h"

namespace koru
{
namespace detail
{
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
    KORU_defctor(sync_task, = delete;);

    template <class>
    struct promise : base {
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
        constexpr void return_void() noexcept {}
    };

    using promise_type = promise<T>;
};
} // namespace detail
using detail::sync_task;
} // namespace koru