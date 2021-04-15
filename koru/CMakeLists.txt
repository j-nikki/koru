add_library(koru STATIC "src/koru.cpp")
target_include_directories(koru INTERFACE "include")
target_sources(koru INTERFACE "koru.natvis")