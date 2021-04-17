#include "../include/koru.h"

namespace koru::detail
{
void throw_last_winapi_error()
{
    throw std::system_error{static_cast<int>(GetLastError()),
                            std::system_category()};
}
} // namespace koru::detail