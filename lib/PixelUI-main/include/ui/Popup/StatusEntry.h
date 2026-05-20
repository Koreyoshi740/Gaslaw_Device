#pragma once

#include <functional>
#include <stddef.h>

struct StatusEntry {
    const char* label;
    std::function<void(char* buf, size_t n)> formatter;
};
