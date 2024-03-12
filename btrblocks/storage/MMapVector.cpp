#include "MMapVector.hpp"
using namespace std;

// -------------------------------------------------------------------------------------
void btrblocks::writeBinary(const char* pathname, std::vector<std::string>& v) {
  using Data = Vector<std::string_view>::Data;
  // std::cout << "Writing binary file : " << pathname << std::endl;
  int fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  die_if(fd != -1);
  uint64_t fileSize = 8 + 16 * v.size();
  for (const auto& s : v) {
    fileSize += s.size() + 1;
  }
#if defined(__linux__)
  die_if(posix_fallocate(fd, 0, fileSize) == 0);
#elif defined(__APPLE__)
  fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, static_cast<off_t>(fileSize)};
  // Try to get a continous chunk of disk space
  int ret = fcntl(fd, F_PREALLOCATE, &store);
  if(-1 == ret){
    // OK, perhaps we are too fragmented, allocate non-continuous
    store.fst_flags = F_ALLOCATEALL;
    ret = fcntl(fd, F_PREALLOCATE, &store);
    if (-1 == ret)
      die_if(false);
  }
  die_if(0 == ftruncate(fd, fileSize));
#endif
  auto data =
      reinterpret_cast<Data*>(mmap(nullptr, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  data->count = v.size();
  die_if(data != MAP_FAILED);
  uint64_t offset = 8 + 16 * v.size();
  char* dst = reinterpret_cast<char*>(data);
  uint64_t slot = 0;
  for (auto s : v) {
    data->slot[slot].size = s.size();
    data->slot[slot].offset = offset;
    strcpy(dst + offset, s.data());
    offset += s.size();
    slot++;
  }
  die_if(close(fd) == 0);
}
