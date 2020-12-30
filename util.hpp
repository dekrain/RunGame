#pragma once

#include <memory>

struct FileContents {
    std::unique_ptr<unsigned char[]> data;
    size_t size;
};

FileContents ReadFile(char const* fname, bool binary = true);
void WriteFile(char const* fname, unsigned char const* data, size_t size, bool binary = true);
