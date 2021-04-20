#pragma push_macro("INFINITE")
#pragma push_macro("ERROR_IO_PENDING")
#pragma push_macro("GENERIC_READ")
#pragma push_macro("GENERIC_WRITE")
#pragma push_macro("FILE_FLAG_OVERLAPPED")
#pragma push_macro("FILE_ATTRIBUTE_NORMAL")
#pragma push_macro("FILE_SHARE_READ")
#pragma push_macro("FILE_SHARE_WRITE")
#pragma push_macro("FILE_SHARE_DELETE")
#pragma push_macro("CREATE_NEW")
#pragma push_macro("CREATE_ALWAYS")
#pragma push_macro("OPEN_EXISTING")
#pragma push_macro("OPEN_ALWAYS")
#pragma push_macro("TRUNCATE_EXISTING")
#pragma push_macro("MAXIMUM_WAIT_OBJECTS")
#pragma push_macro("INVALID_HANDLE_VALUE")
#pragma push_macro("STATUS_WAIT_0")
#pragma push_macro("WAIT_OBJECT_0")
#pragma push_macro("AF_UNSPEC")
#pragma push_macro("AF_INET")
#pragma push_macro("AF_INET6")
#pragma push_macro("SOCK_STREAM")
#pragma push_macro("SOCK_DGRAM")
#pragma push_macro("IPPROTO_TCP")
#pragma push_macro("IPPROTO_UDP")
#pragma push_macro("WSADESCRIPTION_LEN")
#pragma push_macro("WSASYS_STATUS_LEN")
#pragma push_macro("MAKEWORD")
#pragma warning(push)
#pragma warning(disable : 4005) /* macro redefinition */
#define INFINITE 0xFFFFFFFF     // Infinite timeout
#define ERROR_IO_PENDING 997L   // dderror
#define GENERIC_READ (0x80000000L)
#define GENERIC_WRITE (0x40000000L)
#define FILE_FLAG_OVERLAPPED 0x40000000
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define FILE_SHARE_DELETE 0x00000004
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
#define MAXIMUM_WAIT_OBJECTS 64 // Maximum number of wait objects
#define INVALID_HANDLE_VALUE                                                   \
    ((::koru::detail::HANDLE)(::koru::detail::LONG_PTR)-1)
#define STATUS_WAIT_0 ((::koru::detail::DWORD)0x00000000L)
#define WAIT_OBJECT_0 ((STATUS_WAIT_0) + 0)
#define AF_UNSPEC 0    // unspecified
#define AF_INET 2      // internetwork: UDP, TCP, etc.
#define AF_INET6 23    // Internetwork Version 6
#define SOCK_STREAM 1  /* stream socket */
#define SOCK_DGRAM 2   /* datagram socket */
#define IPPROTO_TCP 6  /* tcp */
#define IPPROTO_UDP 17 /* user datagram protocol */
#define WSADESCRIPTION_LEN 256
#define WSASYS_STATUS_LEN 128
#define MAKEWORD(low, high)                                                    \
    ((::koru::detail::WORD)(                                                   \
        ((::koru::detail::BYTE)(low)) |                                        \
        ((::koru::detail::WORD)((::koru::detail::BYTE)(high))) << 8))
#pragma warning(pop)