#include <koru.h>

koru::sync_task<void> write_hash(koru::context<> &ctx, const wchar_t *src,
                                 const wchar_t *dst)
{
    std::size_t h;
    { // Calculate hash from contents of src
        printf("%S: init\n", src);
        auto f          = ctx.open(src);
        auto sz         = GetFileSize(f.native_handle, nullptr);
        auto buf        = std::make_unique_for_overwrite<char[]>(sz);
        auto bytes_read = co_await ctx.read(f.at(0), buf.get(), sz);
        h = std::hash<std::string_view>{}({buf.get(), bytes_read});
        printf("%S: hash=%llu\n", src, h);
    }
    { // Write string representation of hash to dst
        auto f = ctx.open(dst, koru::access::write);
        char buf[32], *p = &buf[32];
        for (; h; h /= 10)
            *--p = '0' + static_cast<char>(h % 10);
        co_await ctx.write(f.at(0), p,
                           static_cast<DWORD>(std::distance(p, &buf[32])));
        printf("%S: wrote hash to %S\n", src, dst);
    }
}

int main()
{
    wchar_t path[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, path);
    try {
        koru::context<> ctx;
        auto f1 = write_hash(ctx, LR"(..\..\..\..\CMakeLists.txt)", L"h1.txt");
        auto f2 = write_hash(ctx, LR"(..\..\..\..\.clang-format)", L"h2.txt");
        ctx.run();
        f1.get();
        f2.get();
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}