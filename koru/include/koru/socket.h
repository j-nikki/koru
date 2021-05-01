//
// SOCKET : A communication endpoint descriptor
//

#pragma once

#include "detail/winapi.h"

#include "detail/win_macros_begin.inl"

namespace koru
{
namespace detail
{
struct inet_info {
    int family, socktype, protocol;
};

class socket
{
    template <bool, bool, std::size_t>
    friend class context;

    socket(detail::SOCKET s) : s_{s} {}

  public:
    KORU_defctor(socket, = delete;);
    ~socket()
    {
        [[maybe_unused]] const auto res = closesocket(s_);
        KORU_assert(res == 0);
    }

  private:
    detail::SOCKET s_;
};
} // namespace detail

static constexpr inline detail::inet_info //
    tcp{AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP},
    tcp4{AF_INET, SOCK_STREAM, IPPROTO_TCP},
    tcp6{AF_INET6, SOCK_STREAM, IPPROTO_TCP},
    udp{AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP},
    udp4{AF_INET, SOCK_DGRAM, IPPROTO_UDP},
    udp6{AF_INET6, SOCK_DGRAM, IPPROTO_UDP};

} // namespace koru

#include "detail/win_macros_end.inl"