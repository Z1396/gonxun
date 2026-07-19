/**
 * @file shared_memory.cpp
 * @brief POSIX 共享内存 IPC 实现文件
 * 
 * @details 本文件实现了基于 POSIX 的共享内存进程间通信功能。
 *          核心特性：
 *          - POSIX API：使用 shm_open, mmap, munmap 等系统调用
 *          - 进程间通信：多个进程可共享同一块内存区域
 *          - 零拷贝：数据传输无需复制，效率高
 *          - 同步机制：需要外部同步（如信号量、互斥锁）
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，实现基础共享内存功能
 *       
 * @note POSIX 共享内存 API：
 *       - shm_open(): 创建或打开共享内存对象
 *       - ftruncate(): 设置共享内存大小
 *       - mmap(): 将共享内存映射到进程地址空间
 *       - munmap(): 解除内存映射
 *       - close(): 关闭文件描述符
 *       - shm_unlink(): 删除共享内存对象
 *       
 * @note 使用流程：
 *       1. 创建/打开：SharedMemory shm("/myshm", 1024, true);
 *       2. 写入数据：shm.write(data, size, offset);
 *       3. 读取数据：shm.read(buffer, size, offset);
 *       4. 自动清理：析构函数自动调用 munmap 和 close
 *       
 * @note 线程安全警告：
 *       - POSIX 共享内存本身不提供同步机制
 *       - 如果多个进程/线程同时访问，需要外部同步
 *       - 推荐使用信号量或互斥锁保护共享数据
 *       
 * @note 性能优势：
 *       - 零拷贝：数据直接在内存中共享
 *       - 高吞吐量：适合大量数据传输
 *       - 低延迟：无需系统调用和数据复制
 *       
 * @warning 共享内存名称必须以 '/' 开头（如 "/gonxun_vision"）
 *          
 * @see shared_memory.hpp
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

/**
 * @brief 构造函数，创建或打开共享内存
 * 
 * @details 使用 POSIX API 创建或打开共享内存对象，并映射到进程地址空间。
 *          
 * @param name 共享内存名称（必须以 '/' 开头，如 "/gonxun_vision"）
 * @param size 共享内存大小（字节）
 * @param create 是否创建新的共享内存（true=创建，false=打开已存在的）
 * 
 * @note 创建流程：
 *       1. 打开或创建共享内存对象（shm_open）
 *       2. 设置大小（ftruncate）
 *       3. 映射到进程地址空间（mmap）
 *       4. 初始化为 0（仅创建时）
 *       
 * @note 权限设置：
 *       - O_RDWR: 读写权限
 *       - O_CREAT: 创建标志（仅创建时）
 *       - 0666: 文件权限（所有用户可读写）
 *       
 * @note 错误处理：
 *       - shm_open 失败：输出错误信息，fd_ = -1
 *       - ftruncate 失败：关闭文件描述符，fd_ = -1
 *       - mmap 失败：关闭文件描述符，ptr_ = nullptr
 *       
 * @warning 如果 create=true 且共享内存已存在，会先删除旧的共享内存。
 *          这可能导致其他进程访问失败。
 */
SharedMemory::SharedMemory(const std::string& name, size_t size, bool create)
    : name_(name), size_(size)
{
    // 步骤 1: 打开或创建共享内存
    int flags = O_RDWR;  // 读写权限
    if (create) {
        flags |= O_CREAT;  // 创建标志
        // 如果已存在则截断（删除旧的）
        shm_unlink(name_.c_str());
    }

    // 调用 shm_open 系统调用
    fd_ = shm_open(name_.c_str(), flags, 0666);
    if (fd_ < 0) {
        std::cerr << "[SharedMemory] shm_open 失败: " << strerror(errno) << std::endl;
        return;
    }

    // 步骤 2: 设置大小（仅创建时）
    if (create && ftruncate(fd_, size_) < 0) {
        std::cerr << "[SharedMemory] ftruncate 失败: " << strerror(errno) << std::endl;
        close(fd_);
        fd_ = -1;
        return;
    }

    // 步骤 3: 映射到进程地址空间
    ptr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr_ == MAP_FAILED) {
        std::cerr << "[SharedMemory] mmap 失败: " << strerror(errno) << std::endl;
        ptr_ = nullptr;
        close(fd_);
        fd_ = -1;
        return;
    }

    // 步骤 4: 初始化为 0（仅创建时）
    if (create) {
        memset(ptr_, 0, size_);
    }

    std::cout << "[SharedMemory] 创建成功: " << name_ << " (" << size_ << " bytes)" << std::endl;
}

/**
 * @brief 析构函数，释放共享内存资源
 * 
 * @details 解除内存映射并关闭文件描述符。
 *          不会删除共享内存对象（允许其他进程继续访问）。
 *          
 * @note 清理流程：
 *       1. 调用 munmap 解除映射
 *       2. 调用 close 关闭文件描述符
 *       3. 不调用 shm_unlink（保留共享内存对象）
 *       
 * @warning 如果需要删除共享内存对象，请显式调用 shm_unlink()。
 *          删除后其他进程将无法访问该共享内存。
 */
SharedMemory::~SharedMemory()
{
    // 解除内存映射
    if (ptr_ && ptr_ != MAP_FAILED) {
        munmap(ptr_, size_);
        ptr_ = nullptr;
    }

    // 关闭文件描述符
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }

    // 可选：删除共享内存（默认不删除）
    // shm_unlink(name_.c_str());
}

/**
 * @brief 写入数据到共享内存
 * 
 * @details 将数据复制到共享内存的指定偏移位置。
 *          
 * @param data 源数据指针
 * @param size 数据大小（字节）
 * @param offset 偏移位置（字节，默认 0）
 * 
 * @return true 写入成功
 * @return false 写入失败（无效状态或超出范围）
 *         
 * @warning 此函数不是线程安全的！
 *          如果多个进程/线程同时写入，需要外部同步机制。
 */
bool SharedMemory::write(const void* data, size_t size, size_t offset)
{
    // 检查有效性
    if (!isValid() || offset + size > size_) {
        return false;
    }
    
    // 使用 memcpy 复制数据到共享内存
    memcpy(static_cast<char*>(ptr_) + offset, data, size);
    return true;
}

/**
 * @brief 从共享内存读取数据
 * 
 * @details 从共享内存的指定偏移位置读取数据到缓冲区。
 *          
 * @param data 目标缓冲区指针
 * @param size 数据大小（字节）
 * @param offset 偏移位置（字节，默认 0）
 * 
 * @return true 读取成功
 * @return false 读取失败（无效状态或超出范围）
 *         
 * @warning 此函数不是线程安全的！
 *          如果多个进程/线程同时读写，需要外部同步机制。
 */
bool SharedMemory::read(void* data, size_t size, size_t offset)
{
    // 检查有效性
    if (!isValid() || offset + size > size_) {
        return false;
    }
    
    // 使用 memcpy 从共享内存复制数据到缓冲区
    memcpy(data, static_cast<char*>(ptr_) + offset, size);
    return true;
}

} // namespace gonxun