# Koru
Koru (from Finglish *korutiini*) is a library that facilitates using overlapped (asynchronous) I/Os on Windows. It uses C++20 coroutines to keep the library use site coherent – no alternative will be provided as this project is specifically meant to demonstrate the new of C++20.

In the 2008 book Concurrent Programming on Windows by Joe Duffy, different models of notifying an awaiter of an overlapped I/O completion are presented. IOCP was presented as the go-to rendezvous mechanism for any "serious" async I/O. `WaitForMultipleObjects`, the function this library uses, has a limit of waiting on only up to `MAXIMUM_WAIT_OBJECTS` handles\* and engages no thread pools / APCs, latter possibly being a pro in the sense that it models coöperative multitasking - no control mechanisms to govern access on shared resources, for example, is required.

On the account of aforementioned, it is evident that the approach opted for here does not scale, but well suffices in demonstrating yet another use case for C++20 coroutines.

\*It is actually possible to wait on more than `MAXIMUM_WAIT_OBJECTS` handles, say, by creating `⌈N/MAXIMUM_WAIT_OBJECTS⌉` threads, where `N` is the number of handles to wait on, and each thread waits on up to `MAXIMUM_WAIT_OBJECTS` handles.

## Demo

An abridgement of `write_hash` in `test/hash.cpp`:
```c++
koru::sync_task<void> write_hash(koru::context<> &ctx, auto src, auto dst) {
    std::size_t h;
    { // Calculate hash from contents of src
        auto f          = ctx.open(src);
        auto sz         = GetFileSize(f.native_handle, nullptr);
        auto buf        = std::make_unique_for_overwrite<char[]>(sz);
        auto bytes_read = co_await ctx.read(f.at(0), buf.get(), sz);
        h = std::hash<std::string_view>{}({buf.get(), bytes_read});
    }
    { // Write string representation of hash to dst
        auto f = ctx.open(dst, koru::access::write);
        char buf[32], *p = &buf[32];
        for (; h; h /= 10)
            *--p = '0' + static_cast<char>(h % 10);
        co_await ctx.write(f.at(0), p,
                           static_cast<DWORD>(std::distance(p, &buf[32])));
    }
}
```

## Using in your project

Please see `LICENSE` for terms and conditions of use.

The easiest way to use Koru in your project is to include it as a submodule: `git submodule add git@github.com:j-nikki/koru.git`. Now discover Koru in your CMake file (so you can `#include <koru.h>` in C++):
```cmake
add_subdirectory("koru")
target_link_libraries(<target-name> ... koru)
```