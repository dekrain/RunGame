#include "util.hpp"

#include <fstream>

FileContents ReadFile(char const* fname, bool binary) {
    auto file = std::ifstream(fname, std::ios::in | (binary ? std::ios::binary : std::ios::openmode()));
    file.seekg(0, std::ios::end);
    FileContents contents;
    contents.size = file.tellg();
    file.seekg(0);
    contents.data = std::unique_ptr<unsigned char[]>(new unsigned char[contents.size]);
    file.read(reinterpret_cast<char*>(contents.data.get()), contents.size);
    return contents;
}

void WriteFile(char const* fname, unsigned char const* data, size_t size, bool binary) {
    auto file = std::ofstream(fname, std::ios::out | (binary ? std::ios::binary : std::ios::openmode()));
    file.write(reinterpret_cast<char const*>(data), size);
}
