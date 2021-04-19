#pragma once

#include "winapi.h"
#include <exception>
#include <stdexcept>
#include <type_traits>

namespace koru::detail
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

#define KORU_defctor(Cls, ...)                                                 \
    Cls() __VA_ARGS__ Cls(const Cls &) = delete;                               \
    Cls(Cls &&)                        = delete;                               \
    Cls &operator=(const Cls &) = delete;                                      \
    Cls &operator=(Cls &&) = delete

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
    KORU_defctor(lock, = delete;);
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
} // namespace koru::detail