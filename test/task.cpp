//
// Simple test case for file I/O
//

#include <charconv>
#include <filesystem>
#include <koru/all.h>
#include <semaphore>

#pragma warning(push, 3)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#pragma warning(pop)

// TODO: figure out why these warnings happen
#pragma warning(disable : 4626 5027)

koru::sync_task<int> foo()
{
    co_return 42;
}

koru::sync_task<int> bar()
{
    co_return co_await foo() * 2;
}

koru::sync_task<int> baz(std::binary_semaphore &s)
{
    struct thread_switch : std::suspend_always {
        thread_switch(std::binary_semaphore &s) : s{s} {}
        KORU_defctor(thread_switch, = delete;);
        std::binary_semaphore &s;
        void await_suspend(std::coroutine_handle<> h)
        {
            std::thread{[h, &s = s] {
                h.resume();
                s.release();
            }}.detach();
        }
    };
    co_await thread_switch{s};
    co_return 42;
}

TEST_CASE("task awaiting works")
{
    SUBCASE("task::get() works") { REQUIRE_EQ(foo().get(), 42); }
    SUBCASE("task awaiting works") { REQUIRE_EQ(bar().get(), 42 * 2); }
    SUBCASE("thread-switching works")
    {
        std::binary_semaphore s{0};
        auto coro = baz(s);
        s.acquire();
        REQUIRE_EQ(coro.get(), 42);
    }
}