#include "../include/koru/detail/utils.h"
#include "../include/koru/detail/winapi.h"
#include <algorithm>
#include <system_error>

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#define _WIN32_WINNT 0x0600 /* minimum for SRWLs */
#endif

#pragma warning(push, 3)
#define WIN32_LEAN_AND_MEAN
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#include <iphlpapi.h>
#pragma warning(pop)

#pragma comment(lib, "Ws2_32.lib")

const std::error_category &wsa_category() noexcept;

namespace koru::detail
{
void throw_last_winapi_error()
{
    throw std::system_error{static_cast<int>(GetLastError()),
                            std::system_category()};
}
void throw_last_wsa_error()
{
    throw std::system_error{static_cast<int>(WSAGetLastError()),
                            wsa_category()};
}

SOCKET create_socket(const wchar_t *node, const wchar_t *service,
                     const ADDRINFOW &hints)
{
    ::ADDRINFOW *res;
    if (GetAddrInfoW(node, service, std::bit_cast<const ::ADDRINFOW *>(&hints),
                     &res) != 0)
        throw_last_wsa_error();
    KORU_defer[=] { FreeAddrInfoW(res); };
    do {
        const auto sock =
            WSASocketW(res->ai_family, res->ai_socktype, res->ai_protocol,
                       nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (sock != INVALID_SOCKET)
            return {sock};
    } while ((res = res->ai_next));
    throw_last_wsa_error();
}

#pragma region WinAPI glue
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

int WSAStartup(WORD wVersionRequested, WSADATA *lpWSAData) noexcept
{
    return ::WSAStartup(wVersionRequested,
                        std::bit_cast<::WSADATA *>(lpWSAData));
}
#pragma endregion
} // namespace koru::detail

#pragma region wsa codes
// From here:
// https://docs.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2

/// l = re.findall(r'\b(\d+?)<\/dt>\s*<\/dl><\/td>\s*<td><dl>\s*<dt><span id=\"([^\"]+?)\"',pyperclip.paste())
// l = list(sorted((int(x[0]),x[1]) for x in l)))
// ','.join(str(x[0]) for x in l)   # codes
// ','.join(f'"{x[1]}"' for x in l) # strings

const int wsa_codes[]{
    6,     8,     87,    995,   996,   10004, 10009, 10013, 10014, 10022, 10024,
    10035, 10036, 10037, 10038, 10039, 10040, 10041, 10042, 10043, 10044, 10045,
    10046, 10047, 10048, 10049, 10050, 10051, 10052, 10053, 10054, 10055, 10056,
    10057, 10058, 10059, 10060, 10061, 10062, 10063, 10064, 10065, 10066, 10067,
    10068, 10069, 10070, 10071, 10091, 10092, 10093, 10101, 10102, 10103, 10104,
    10105, 10106, 10107, 10108, 10109, 10110, 10111, 10112, 11001, 11002, 11003,
    11004, 11005, 11006, 11007, 11008, 11009, 11010, 11011, 11012, 11013, 11014,
    11015, 11016, 11017, 11018, 11019, 11020, 11021, 11022, 11023, 11024, 11025,
    11026, 11027, 11028, 11029, 11030, 11031};
const char *wsa_strs[]{"Specified_event_object_handle_is_invalid.",
                       "Insufficient_memory_available.",
                       "One_or_more_parameters_are_invalid.",
                       "Overlapped_operation_aborted.",
                       "Overlapped_I_O_event_object_not_in_signaled_state.",
                       "Interrupted_function_call.",
                       "File_handle_is_not_valid.",
                       "Permission_denied.",
                       "Bad_address.",
                       "Invalid_argument.",
                       "Too_many_open_files.",
                       "Resource_temporarily_unavailable.",
                       "Operation_now_in_progress.",
                       "Operation_already_in_progress.",
                       "Socket_operation_on_nonsocket.",
                       "Destination_address_required.",
                       "Message_too_long.",
                       "Protocol_wrong_type_for_socket.",
                       "Bad_protocol_option.",
                       "Protocol_not_supported.",
                       "Socket_type_not_supported.",
                       "Operation_not_supported.",
                       "Protocol_family_not_supported.",
                       "Address_family_not_supported_by_protocol_family.",
                       "Address_already_in_use.",
                       "Cannot_assign_requested_address.",
                       "Network_is_down.",
                       "Network_is_unreachable.",
                       "Network_dropped_connection_on_reset.",
                       "Software_caused_connection_abort.",
                       "Connection_reset_by_peer.",
                       "No_buffer_space_available.",
                       "Socket_is_already_connected.",
                       "Socket_is_not_connected.",
                       "Cannot_send_after_socket_shutdown.",
                       "Too_many_references.",
                       "Connection_timed_out.",
                       "Connection_refused.",
                       "Cannot_translate_name.",
                       "Name_too_long.",
                       "Host_is_down.",
                       "No_route_to_host.",
                       "Directory_not_empty.",
                       "Too_many_processes.",
                       "User_quota_exceeded.",
                       "Disk_quota_exceeded.",
                       "Stale_file_handle_reference.",
                       "Item_is_remote.",
                       "Network_subsystem_is_unavailable.",
                       "Winsock.dll_version_out_of_range.",
                       "Successful_WSAStartup_not_yet_performed.",
                       "Graceful_shutdown_in_progress.",
                       "No_more_results.",
                       "Call_has_been_canceled.",
                       "Procedure_call_table_is_invalid.",
                       "Service_provider_is_invalid.",
                       "Service_provider_failed_to_initialize.",
                       "System_call_failure.",
                       "Service_not_found.",
                       "Class_type_not_found.",
                       "No_more_results.",
                       "Call_was_canceled.",
                       "Database_query_was_refused.",
                       "Host_not_found.",
                       "Nonauthoritative_host_not_found.",
                       "This_is_a_nonrecoverable_error.",
                       "Valid_name__no_data_record_of_requested_type.",
                       "QoS_receivers.",
                       "QoS_senders.",
                       "No_QoS_senders.",
                       "QoS_no_receivers.",
                       "QoS_request_confirmed.",
                       "QoS_admission_error.",
                       "QoS_policy_failure.",
                       "QoS_bad_style.",
                       "QoS_bad_object.",
                       "QoS_traffic_control_error.",
                       "QoS_generic_error.",
                       "QoS_service_type_error.",
                       "QoS_flowspec_error.",
                       "Invalid_QoS_provider_buffer.",
                       "Invalid_QoS_filter_style.",
                       "Invalid_QoS_filter_type.",
                       "Incorrect_QoS_filter_count.",
                       "Invalid_QoS_object_length.",
                       "Incorrect_QoS_flow_count.",
                       "Unrecognized_QoS_object.",
                       "Invalid_QoS_policy_object.",
                       "Invalid_QoS_flow_descriptor.",
                       "Invalid_QoS_provider-specific_flowspec.",
                       "Invalid_QoS_provider-specific_filterspec.",
                       "Invalid_QoS_shape_discard_mode_object.",
                       "Invalid_QoS_shaping_rate_object.",
                       "Reserved_policy_QoS_element_type."};
const std::error_category &wsa_category() noexcept
{
    static struct R : std::error_category {
        const char *name() const noexcept override
        {
            return "Windows Sockets error code";
        }
        std::string message(int code) const override
        {
            const auto f = std::begin(wsa_codes), l = std::end(wsa_codes);
            const auto it = std::find(f, l, code);
            KORU_assert(it != l);
            return wsa_strs[std::distance(f, it)];
        }
    } r;
    return r;
}
#pragma endregion