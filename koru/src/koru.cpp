#include "../include/koru/context.h"

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#define _WIN32_WINNT 0x0600 /* minimum for SRWLs */
#endif

#pragma warning(push, 3)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma warning(pop)

namespace koru::detail
{
void throw_last_winapi_error()
{
    throw std::system_error{static_cast<int>(GetLastError()),
                            std::system_category()};
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead, OVERLAPPED *lpOverlapped) noexcept
{
    return ::ReadFile(hFile, lpBuffer, nNumberOfBytesToRead,
                      lpNumberOfBytesRead,
                      std::bit_cast<::OVERLAPPED *>(lpOverlapped));
}
BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten,
               OVERLAPPED *lpOverlapped) noexcept
{
    return ::WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite,
                       lpNumberOfBytesWritten,
                       std::bit_cast<::OVERLAPPED *>(lpOverlapped));
}

void InitializeSRWLock(SRWLOCK *SRWLock) noexcept
{
    ::InitializeSRWLock(std::bit_cast<::SRWLOCK *>(SRWLock));
}
void ReleaseSRWLockExclusive(SRWLOCK *SRWLock) noexcept
{
    ::ReleaseSRWLockExclusive(std::bit_cast<::SRWLOCK *>(SRWLock));
}
void ReleaseSRWLockShared(SRWLOCK *SRWLock) noexcept
{
    ::ReleaseSRWLockShared(std::bit_cast<::SRWLOCK *>(SRWLock));
}
void AcquireSRWLockExclusive(SRWLOCK *SRWLock) noexcept
{
    ::AcquireSRWLockExclusive(std::bit_cast<::SRWLOCK *>(SRWLock));
}
void AcquireSRWLockShared(SRWLOCK *SRWLock) noexcept
{
    ::AcquireSRWLockShared(std::bit_cast<::SRWLOCK *>(SRWLock));
}

HANDLE CreateEventW(SECURITY_ATTRIBUTES *lpEventAttributes, BOOL bManualReset,
                    BOOL bInitialState, LPCWSTR lpName) noexcept
{
    return ::CreateEventW(
        std::bit_cast<::SECURITY_ATTRIBUTES *>(lpEventAttributes), bManualReset,
        bInitialState, lpName);
}

HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   SECURITY_ATTRIBUTES *lpSecurityAttributes,
                   DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                   HANDLE hTemplateFile) noexcept
{
    return ::CreateFileW(
        lpFileName, dwDesiredAccess, dwShareMode,
        std::bit_cast<::SECURITY_ATTRIBUTES *>(lpSecurityAttributes),
        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}
} // namespace koru::detail