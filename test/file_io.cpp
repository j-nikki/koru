//
// Simple test case for file I/O
//

#include <charconv>
#include <filesystem>
#include <koru.h>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

using context = koru::context<true>;

koru::sync_task<void> write_hash(context &ctx, const wchar_t *src,
                                 const wchar_t *dst, std::size_t &h)
{
    { // Calculate hash from contents of src
        printf("%S: init\n", src);
        auto f          = ctx.open(src);
        auto sz         = GetFileSize(f.native_handle, nullptr);
        auto buf        = std::make_unique_for_overwrite<char[]>(sz);
        auto bytes_read = co_await ctx.read(f.at(0), buf.get(), sz);
        h = std::hash<std::string_view>{}({buf.get(), bytes_read});
        printf("%S: hash=%zu\n", src, h);
    }
    { // Write string representation of hash to dst
        char buf[32];
        auto sz = snprintf(buf, 32, "%zu", h);
        auto f  = ctx.open(dst, koru::access::write);
        co_await ctx.write(f.at(0), &buf[0], static_cast<DWORD>(sz));
        printf("%S: wrote hash to %S\n", src, dst);
    }
}

koru::sync_task<std::size_t> read_hash(context &ctx, const wchar_t *path)
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

TEST_CASE("expected file hashes get expectedly written")
{
    koru::context<true> ctx;
    std::size_t h1_expected{}, h2_expected{};
    auto f1 =
        write_hash(ctx, LR"(..\..\CMakeLists.txt)", L"h1.txt", h1_expected);
    auto f2 =
        write_hash(ctx, LR"(..\..\.clang-format)", L"h2.txt", h2_expected);
    ctx.run();
    f1.get();
    f2.get();

    SUBCASE("file hashes exist on disk")
    {
        using std::filesystem::exists;
        REQUIRE(exists("h1.txt"));
        REQUIRE(exists("h2.txt"));
    }

    SUBCASE("file hashes are as expected")
    {
        auto h1_actual = read_hash(ctx, L"h1.txt");
        auto h2_actual = read_hash(ctx, L"h2.txt");
        ctx.run();
        REQUIRE_EQ(h1_actual.get(), h1_expected);
        REQUIRE_EQ(h2_actual.get(), h2_expected);
    }
}