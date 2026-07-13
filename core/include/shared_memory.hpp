/**
 * POSIX 共享内存 IPC
 * 用于进程间通信
 */
#pragma once

#include <string>
#include <cstdint>

namespace gonxun {

/**
 * 共享内存类
 * 封装 POSIX shm_open 操作
 */
class SharedMemory {
public:
    /**
     * 构造函数
     * @param name 共享内存名称 (以 / 开头)
     * @param size 共享内存大小 (字节)
     * @param create 是否创建新的共享内存
     */
    SharedMemory(const std::string& name, size_t size, bool create = true);
    ~SharedMemory();

    // 禁止拷贝
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    /**
     * 写入数据到共享内存
     * @param data 数据指针
     * @param size 数据大小
     * @param offset 偏移量
     * @return 是否成功
     */
    bool write(const void* data, size_t size, size_t offset = 0);

    /**
     * 从共享内存读取数据
     * @param data 目标缓冲区
     * @param size 读取大小
     * @param offset 偏移量
     * @return 是否成功
     */
    bool read(void* data, size_t size, size_t offset = 0);

    /**
     * 获取共享内存指针
     */
    void* ptr() { return ptr_; }

    /**
     * 获取共享内存大小
     */
    size_t size() const { return size_; }

    /**
     * 检查是否有效
     */
    bool isValid() const { return fd_ >= 0 && ptr_ != nullptr; }

private:
    std::string name_;
    size_t size_;
    int fd_{-1};
    void* ptr_{nullptr};
};

} // namespace gonxun