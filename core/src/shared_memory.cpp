/**
 * POSIX 共享内存 IPC 实现
 */

#include "shared_memory.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace gonxun {

SharedMemory::SharedMemory(const std::string& name, size_t size, bool create)
    : name_(name), size_(size)
{
    // 打开或创建共享内存
    int flags = O_RDWR;
    if (create) {
        flags |= O_CREAT;
        // 如果已存在则截断
        shm_unlink(name_.c_str());
    }

    fd_ = shm_open(name_.c_str(), flags, 0666);
    if (fd_ < 0) {
        std::cerr << "[SharedMemory] shm_open 失败: " << strerror(errno) << std::endl;
        return;
    }

    // 设置大小
    if (create && ftruncate(fd_, size_) < 0) {
        std::cerr << "[SharedMemory] ftruncate 失败: " << strerror(errno) << std::endl;
        close(fd_);
        fd_ = -1;
        return;
    }

    // 映射到进程地址空间
    ptr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr_ == MAP_FAILED) {
        std::cerr << "[SharedMemory] mmap 失败: " << strerror(errno) << std::endl;
        ptr_ = nullptr;
        close(fd_);
        fd_ = -1;
        return;
    }

    // 初始化为 0
    if (create) {
        memset(ptr_, 0, size_);
    }

    std::cout << "[SharedMemory] 创建成功: " << name_ << " (" << size_ << " bytes)" << std::endl;
}

SharedMemory::~SharedMemory()
{
    if (ptr_ && ptr_ != MAP_FAILED) {
        munmap(ptr_, size_);
        ptr_ = nullptr;
    }

    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }

    // 可选：删除共享内存
    // shm_unlink(name_.c_str());
}

bool SharedMemory::write(const void* data, size_t size, size_t offset)
{
    if (!isValid() || offset + size > size_) {
        return false;
    }
    memcpy(static_cast<char*>(ptr_) + offset, data, size);
    return true;
}

bool SharedMemory::read(void* data, size_t size, size_t offset)
{
    if (!isValid() || offset + size > size_) {
        return false;
    }
    memcpy(data, static_cast<char*>(ptr_) + offset, size);
    return true;
}

} // namespace gonxun