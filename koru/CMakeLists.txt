add_library(koru STATIC "src/koru.cpp" "include/koru/sync_task.h" "include/koru/all.h" "include/koru/detail/utils.h")
target_include_directories(koru INTERFACE "include")
target_sources(koru INTERFACE "koru.natvis")
