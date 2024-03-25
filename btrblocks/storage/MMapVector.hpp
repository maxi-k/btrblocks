#pragma once
// -------------------------------------------------------------------------------------
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
// -------------------------------------------------------------------------------------
#include "common/Exceptions.hpp"
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
inline namespace mmapvector {
// -------------------------------------------------------------------------------------
template <class T>
struct Vector {
  uint64_t count;
  T* data;

  Vector() : data(nullptr) {}
  explicit Vector(uint64_t count) : count(count), data(new T[count]) {}
  explicit Vector(const char* pathname) { readBinary(pathname); }
  Vector(const Vector&) = delete;
  Vector(Vector&& o) noexcept : count(o.count), data(o.data) {
    o.count = 0;
    o.data = nullptr;
  }
  ~Vector() {
    if (data) {
      delete[] data;
    }
  }

  void readBinary(const char* pathname) {
    std::cout << "Reading binary file : " << pathname << std::endl;
    int fd = open(pathname, O_RDONLY);
    if (fd == -1) {
      cout << pathname << endl;
    }
    die_if(fd != -1);
    struct stat sb;
    die_if(fstat(fd, &sb) != -1);
    count = static_cast<uint64_t>(sb.st_size) / sizeof(T);
    std::cout << "File count : " << count << std::endl;
    data = new T[count];
    std::cout << "Read binary file : " << pathname << std::endl;
    uint64_t read_count = read(fd, data, sb.st_size);
    std::cout << "File count : " << read_count << " should be " << sb.st_size << std::endl;
    die_if(read(fd, data, sb.st_size) == sb.st_size);
    std::cout << "try close: " << pathname << std::endl;
    die_if(close(fd) == 0);
  }

  [[nodiscard]] uint64_t size() const { return count; }
  T& operator[](std::size_t idx) { return data[idx]; }
  const T& operator[](std::size_t idx) const { return data[idx]; }

  // ------------------------------------------------------------------------------
  // iteration
  // ------------------------------------------------------------------------------
  using iterator = T*;
  using const_iterator = const T*;
  // ------------------------------------------------------------------------------
  iterator begin() { return data; }
  iterator end() { return data + count; }
  [[nodiscard]] const_iterator begin() const { return data; }
  [[nodiscard]] const_iterator end() const { return data + count; }
  // ------------------------------------------------------------------------------
};
// -------------------------------------------------------------------------------------
template <class T>
void writeBinary(const char* pathname, std::vector<T>& v) {
  std::cout << "Writing binary file : " << pathname << std::endl;
  int fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  die_if(fd != -1);
  uint64_t length = v.size() * sizeof(T);
  die_if(posix_fallocate(fd, 0, length) == 0);
  T* data = reinterpret_cast<T*>(mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  die_if(data != MAP_FAILED);
  memcpy(data, v.data(), length);
  die_if(close(fd) == 0);
}
// -------------------------------------------------------------------------------------
struct StringIndexSlot {
  uint64_t size;
  uint64_t offset;
};
// -------------------------------------------------------------------------------------
template <>
struct Vector<std::string_view> {
  struct Data {
    uint64_t count;
    StringIndexSlot slot[];
  };

  uint64_t fileSize;
  Data* data;

  Vector() : data(nullptr) {}
  explicit Vector(const char* pathname) { readBinary(pathname); }
  Vector(const Vector&) = delete;
  Vector(Vector&& o) noexcept : fileSize(o.fileSize), data(o.data) {
    o.fileSize = 0;
    o.data = nullptr;
  }
  ~Vector() {
    if (data) {
      die_if(munmap(data, fileSize) == 0);
    }
  }

  void readBinary(const char* pathname) {
    //      std::cout << "Reading binary file : " << pathname << std::endl;
    int fd = open(pathname, O_RDONLY);
    if (fd == -1) {
      cout << pathname << endl;
    }
    die_if(fd != -1);
    struct stat sb;
    die_if(fstat(fd, &sb) != -1);
    fileSize = static_cast<uint64_t>(sb.st_size);
    data = reinterpret_cast<Data*>(mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0));
    die_if(data != MAP_FAILED);
    die_if(close(fd) == 0);
  }

  [[nodiscard]] uint64_t size() const { return data->count; }
  std::string_view operator[](std::size_t idx) const {
    auto slot = data->slot[idx];
    return {reinterpret_cast<char*>(data) + slot.offset, slot.size};
  }
  // ------------------------------------------------------------------------------
  // iteration
  // ------------------------------------------------------------------------------
  template <typename SlotPtr, typename Ptr>
  struct _iterator {
    SlotPtr slot;
    Ptr data;

    _iterator(SlotPtr slot, Ptr data) : slot(slot), data(data) {}
    _iterator& operator++() {
      slot++;
      return *this;
    }
    _iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }
    bool operator!=(const _iterator& o) const { return slot != o.slot; }
    std::string_view operator*() const { return {data, slot->size}; }
  };
  using iterator = _iterator<StringIndexSlot*, char*>;
  using const_iterator = _iterator<const StringIndexSlot*, const char*>;
  // ------------------------------------------------------------------------------
  iterator begin() { return {data->slot, reinterpret_cast<char*>(data)}; }
  iterator end() { return {data->slot + data->count, reinterpret_cast<char*>(data)}; }
  [[nodiscard]] const_iterator begin() const { return {data->slot, reinterpret_cast<char*>(data)}; }
  [[nodiscard]] const_iterator end() const {
    return {data->slot + data->count, reinterpret_cast<char*>(data)};
  }
  // ------------------------------------------------------------------------------
};
// -------------------------------------------------------------------------------------
void writeBinary(const char* pathname, std::vector<std::string>& v);
// -------------------------------------------------------------------------------------
}  // namespace mmapvector
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
