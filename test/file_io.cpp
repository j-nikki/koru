//
// Simple test case for file I/O
//

#include <charconv>
#include <filesystem>
#include <koru.h>
#include <semaphore>

#pragma warning(push, 3)
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#pragma warning(pop)

// TODO: figure out why these warnings happen
#pragma warning(disable : 4626 5027)

koru::sync_task<std::size_t> write_hash(auto &ctx, const wchar_t *src,
                                        const wchar_t *dst)
{
    std::size_t h;
    {
        auto f          = ctx.open(src);
        auto sz         = GetFileSize(f.native_handle, nullptr);
        auto buf        = std::make_unique_for_overwrite<char[]>(sz);
        auto bytes_read = co_await ctx.read(f.at(0), buf.get(), sz);
        h = std::hash<std::string_view>{}({buf.get(), bytes_read});
    }
    {
        char buf[32];
        auto sz = snprintf(buf, 32, "%zu", h);
        auto f  = ctx.open(dst, koru::access::write);
        co_await ctx.write(f.at(0), &buf[0], static_cast<DWORD>(sz));
    }
    co_return h;
}

koru::sync_task<std::size_t> read_hash(auto &ctx, const wchar_t *path)
{
    auto f          = ctx.open(path);
    auto sz         = GetFileSize(f.native_handle, nullptr);
    auto buf        = std::make_unique_for_overwrite<char[]>(sz);
    auto bytes_read = co_await ctx.read(f.at(0), buf.get(), sz);
    std::size_t res{};
    if (auto [_, ec] = std::from_chars(buf.get(), buf.get() + bytes_read, res);
        ec != std::errc{})
        throw std::system_error{std::make_error_code(ec)};
    co_return res;
}

auto init_test_case(auto &ctx)
{
    auto f1 = write_hash(ctx, LR"(..\..\..\CMakeLists.txt)", L"h1.txt");
    auto f2 = write_hash(ctx, LR"(..\..\..\.clang-format)", L"h2.txt");
    ctx.run();
    return std::pair{f1.get(), f2.get()};
}

void for_each_ctx(auto f)
{
    std::binary_semaphore s{0};
    std::exception_ptr ep;
    std::thread t{[&] {
        try {
            f(koru::context<false, false>{});
            f(koru::context<true, false>{});
            // f(koru::context<true, true>{}); // TODO: fix async setting
        } catch (...) {
            ep = std::current_exception();
        }
        s.release();
    }};
    if (!s.try_acquire_for(std::chrono::seconds{5})) {
        TerminateThread(t.native_handle(), 0);
        t.detach();
        struct timeout_error : std::exception {
            using std::exception::exception;
            const char *what() const noexcept { return "for_each_ctx timeout"; }
        };
        throw timeout_error{};
    }
    t.join();
    if (ep)
        std::rethrow_exception(ep);
}

TEST_CASE("expected file hashes get expectedly written")
{
    SUBCASE("file hashes exist on disk")
    {
        for_each_ctx([](auto ctx) {
            init_test_case(ctx);
            REQUIRE(std::filesystem::exists("h1.txt"));
            REQUIRE(std::filesystem::exists("h2.txt"));
            std::filesystem::remove("h1.txt");
            std::filesystem::remove("h2.txt");
        });
    }

    SUBCASE("on-disk file hashes are as expected")
    {
        for_each_ctx([](auto ctx) {
            init_test_case(ctx);
            auto [h1_expected, h2_expected] = init_test_case(ctx);
            auto h1_actual                  = read_hash(ctx, L"h1.txt");
            auto h2_actual                  = read_hash(ctx, L"h2.txt");
            ctx.run();
            REQUIRE_EQ(h1_actual.get(), h1_expected);
            REQUIRE_EQ(h2_actual.get(), h2_expected);
            std::filesystem::remove("h1.txt");
            std::filesystem::remove("h2.txt");
        });
    }
}