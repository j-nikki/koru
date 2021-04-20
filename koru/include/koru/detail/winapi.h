#pragma once

#include <stdint.h>

#include "win_macros_begin.inl"

namespace koru::detail
{

//
// Following forward-declares the subset of WinAPI used by Koru
//

// Finding used WinAPI names (fallible; think comments within string literals):
/// set(m[0] for m in re.finditer(r'//.*|/\*.*?\*/|\b([A-Z][A-Za-z0-9]+|[A-Z][A-Z_]+)\b',pyperclip.paste()) if m[1])

#define KORU_winapi __declspec(dllimport) __stdcall
#define KORU_wsaapi __declspec(dllimport) __stdcall

using ULONG_PTR = uintptr_t;
using UINT_PTR  = uintptr_t;
using LONG_PTR  = intptr_t;
using BYTE      = unsigned char;
using WORD      = unsigned short;
using DWORD     = unsigned long;
using LPDWORD   = DWORD *;
using PVOID     = void *;
using LPVOID    = void *;
using LPCVOID   = const void *;
using HANDLE    = void *;
using BOOL      = int;
using WCHAR     = wchar_t;
using PWSTR     = WCHAR *;
using LPWSTR    = WCHAR *;
using LPCWSTR   = const WCHAR *;
using SOCKET    = UINT_PTR;

struct OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
#pragma warning(suppress : 4201) /* nonstandard nameless struct/union */
        };
        PVOID Pointer;
#pragma warning(suppress : 4201) /* nonstandard nameless struct/union */
    };
    HANDLE hEvent;
};

struct SRWLOCK {
    PVOID Ptr;
};

struct SECURITY_ATTRIBUTES {
    DWORD nLength;
#pragma warning(suppress : 4820) /* padding added after data member */
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
#pragma warning(suppress : 4820) /* padding added after data member */
};

struct WSADATA {
    WORD wVersion;
    WORD wHighVersion;
#ifdef _WIN64
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char *lpVendorInfo;
    char szDescription[WSADESCRIPTION_LEN + 1];
    char szSystemStatus[WSASYS_STATUS_LEN + 1];
#else
    char szDescription[WSADESCRIPTION_LEN + 1];
    char szSystemStatus[WSASYS_STATUS_LEN + 1];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char *lpVendorInfo;
#endif
#pragma warning(suppress : 4820) /* padding added after data member */
};

struct ADDRINFOW {
    int ai_flags;             // AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST
    int ai_family;            // PF_xxx
    int ai_socktype;          // SOCK_xxx
    int ai_protocol;          // 0 or IPPROTO_xxx for IPv4 and IPv6
    size_t ai_addrlen;        // Length of ai_addr
    PWSTR ai_canonname;       // Canonical name for nodename
    struct sockaddr *ai_addr; // Binary address
    ADDRINFOW *ai_next;       // Next structure in linked list
};

// Functions that reference structs in signatures can't be forward-declared

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead,
              koru::detail::OVERLAPPED *lpOverlapped) noexcept;
BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten,
               koru::detail::OVERLAPPED *lpOverlapped) noexcept;

void InitializeSRWLock(koru::detail::SRWLOCK *SRWLock) noexcept;
void ReleaseSRWLockExclusive(koru::detail::SRWLOCK *SRWLock) noexcept;
void ReleaseSRWLockShared(koru::detail::SRWLOCK *SRWLock) noexcept;
void AcquireSRWLockExclusive(koru::detail::SRWLOCK *SRWLock) noexcept;
void AcquireSRWLockShared(koru::detail::SRWLOCK *SRWLock) noexcept;

HANDLE CreateEventW(koru::detail::SECURITY_ATTRIBUTES *lpEventAttributes,
                    BOOL bManualReset, BOOL bInitialState,
                    LPCWSTR lpName) noexcept;

HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   koru::detail::SECURITY_ATTRIBUTES *lpSecurityAttributes,
                   DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                   HANDLE hTemplateFile) noexcept;
int WSAStartup(WORD wVersionRequested, WSADATA *lpWSAData) noexcept;
} // namespace koru::detail

extern "C" {
koru::detail::DWORD KORU_winapi WaitForMultipleObjects(
    koru::detail::DWORD nCount, const koru::detail::HANDLE *lpHandles,
    koru::detail::BOOL bWaitAll, koru::detail::DWORD dwMilliseconds);
koru::detail::DWORD KORU_winapi GetLastError(void);
koru::detail::BOOL KORU_winapi CloseHandle(koru::detail::HANDLE hObject);
koru::detail::BOOL KORU_winapi SetEvent(koru::detail::HANDLE hEvent);
int KORU_wsaapi WSACleanup(void);
int KORU_wsaapi closesocket(koru::detail::SOCKET s);
}

#include "win_macros_end.inl"