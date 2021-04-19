#pragma once

#include <stdint.h>

namespace koru::detail
{

// Finding used WinAPI names (fallible; think comments within string literals):
/// set(m[0] for m in re.finditer(r'//.*|/\*.*?\*/|\b([A-Z][A-Za-z0-9]+|[A-Z][A-Z_]+)\b',pyperclip.paste()) if m[1])

#define KORU_winapi __declspec(dllimport) __stdcall

using ULONG_PTR = uintptr_t;
using LONG_PTR  = intptr_t;
using DWORD     = unsigned long;
using LPDWORD   = DWORD *;
using PVOID     = void *;
using LPVOID    = void *;
using LPCVOID   = const void *;
using HANDLE    = void *;
using BOOL      = int;
using WCHAR     = wchar_t;
using LPWSTR    = WCHAR *;
using LPCWSTR   = const WCHAR *;

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
} // namespace koru::detail

extern "C" {
koru::detail::DWORD KORU_winapi WaitForMultipleObjects(
    koru::detail::DWORD nCount, const koru::detail::HANDLE *lpHandles,
    koru::detail::BOOL bWaitAll, koru::detail::DWORD dwMilliseconds);
koru::detail::DWORD KORU_winapi GetLastError(void);
koru::detail::BOOL KORU_winapi CloseHandle(koru::detail::HANDLE hObject);
koru::detail::BOOL KORU_winapi SetEvent(koru::detail::HANDLE hEvent);
}