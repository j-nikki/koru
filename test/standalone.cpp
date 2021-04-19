//
// Checks syntax, missing headers, etc.
//

#include <koru/all.h>

#include <koru/detail/win_macros_begin.inl>

koru::sync_task<void> foo()
{
    co_return;
}

koru::sync_task<int> bar()
{
    co_return 42;
}

int main()
{
    []<std::size_t... Is>(std::index_sequence<Is...>)
    {
        std::tuple<
            std::tuple<koru::context<false, false, Is + 1>,
                       koru::context<true, true,
                                     std::min<std::size_t>(
                                         Is + 1, MAXIMUM_WAIT_OBJECTS - 1)>,
                       koru::context<true, false, Is + 1>>...>
            t;
    }
    (std::make_index_sequence<MAXIMUM_WAIT_OBJECTS>{});
}

#include <koru/detail/win_macros_end.inl>