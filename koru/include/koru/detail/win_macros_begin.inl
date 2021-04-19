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
#define INFINITE 0xFFFFFFFF   // Infinite timeout
#define ERROR_IO_PENDING 997L // dderror
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
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define STATUS_WAIT_0 ((DWORD)0x00000000L)
#define WAIT_OBJECT_0 ((STATUS_WAIT_0) + 0)