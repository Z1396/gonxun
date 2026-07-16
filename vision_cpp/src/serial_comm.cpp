/**
 * @file serial_comm.cpp
 * @brief 串口通信模块实现文件
 * 
 * @details 本文件实现了智能物流搬运系统的串口通信功能。
 *          核心功能：
 *          - 串口配置：使用 POSIX termios 接口，支持多种波特率
 *          - 后台接收：使用 std::thread 进行后台接收，避免阻塞主线程
 *          - 帧协议：自定义15字节帧结构（帧头+命令+数据+校验+帧尾）
 *          - 模拟模式：支持无串口设备的仿真运行
 *          - 线程安全：使用互斥锁保护共享资源
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，移植自 Python 版本 vision/serial_comm.py
 *       - 2025-02-15: 增加模拟模式和循环测试模式
 *       - 2025-03-20: 优化线程安全设计，增加帧校验机制
 * 
 * @note 帧协议说明：
 *       帧结构（15字节）：
 *       - 帧头(1): 0xAA
 *       - 命令(1): MODE_COLOR/MODE_RING/MODE_DOCK/MODE_QR
 *       - 数据(12): 坐标数据或二维码字符串
 *       - 校验和(1): 命令+数据的累加和低8位
 *       - 帧尾(1): 0x55
 *       
 * @see serial_comm.hpp
 */
#include "serial_comm.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <thread>
#include <unistd.h>

/**
 * @brief 匿名命名空间，包含辅助函数
 */
namespace {

/**
 * @brief 将16位有符号整数拆分为高8位、低8位字节
 * 
 * @details 用于将坐标值（如 X=1234）编码为两个字节：高字节=0x04，低字节=0xD2。
 *          采用大端序（高字节在前）。
 * 
 * @param value 16位有符号整数值（范围：-32768 到 32767）
 * 
 * @return std::pair<uint8_t, uint8_t> 
 *         - first: 高8位字节
 *         - second: 低8位字节
 * 
 * @note 示例：
 *       - value=1234 → {0x04, 0xD2}
 *       - value=256 → {0x01, 0x00}
 *       - value=-1 → {0xFF, 0xFF}
 */
std::pair<uint8_t, uint8_t> packWord(int value) {
    uint16_t v = static_cast<uint16_t>(value);
    return {static_cast<uint8_t>((v & 0xff00) >> 8),  // 高8位
            static_cast<uint8_t>(v & 0xff)};         // 低8位
}

/**
 * @brief 波特率转 termios 速度常量
 * 
 * @details 将常用波特率（如 115200）转换为 POSIX termios 定义的常量（如 B115200）。
 *          如果传入不支持的波特率，默认返回 B115200。
 * 
 * @param baud 波特率（9600、19200、38400、57600、115200、230400、460800、921600）
 * 
 * @return speed_t termios 速度常量
 * 
 * @note 常用波特率：
 *       - 9600: 低速设备（如GPS模块）
 *       - 57600: 中速设备
 *       - 115200: 高速设备（本项目默认）
 *       - 921600: 超高速设备（需硬件支持）
 */
speed_t baudToSpeed(int baud) {
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;  // 默认波特率
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        default:     return B115200;  // 未知波特率，使用默认值
    }
}

}  // namespace

/**
 * @brief 构造函数，初始化串口通信对象
 * 
 * @details 配置串口参数、模拟模式参数，但不立即打开串口。
 *          采用延迟初始化策略，在 start() 时才真正打开串口或启动模拟线程。
 * 
 * @param mock 是否启用模拟模式
 *        - true: 不连接真实串口，使用模拟数据
 *        - false: 连接真实串口设备
 * @param port 串口设备路径（如 "/dev/ttyUSB0" 或 "COM3"）
 * @param baudrate 波特率（如 115200）
 * @param mock_unit 模拟模式下的默认工作模式（仅在 mock_cycle=false 时使用）
 * @param mock_cycle 是否启用循环测试模式
 *        - true: 循环切换所有工作模式（COLOR → RING → DOCK → QR → IDLE）
 *        - false: 固定使用 mock_unit 模式
 * 
 * @note 初始化状态：
 *       - fd_ = -1: 串口文件描述符未打开
 *       - running_ = false: 后台线程未启动
 *       - send_[0] = FRAME_HEADER: 预设帧头
 */
SerialComm::SerialComm(bool mock, const std::string& port, int baudrate,
                       uint8_t mock_unit, bool mock_cycle)
    : mock_(mock),                    // 模拟模式标志
      port_(port),                    // 串口设备路径
      baudrate_(baudrate),            // 波特率
      mock_unit_(mock_unit),          // 模拟默认模式
      mock_cycle_(mock_cycle),        // 循环测试模式标志
      fd_(-1),                        // 文件描述符（未打开）
      receive_(4, 0),                 // 接收缓冲区（4字节）
      send_(15, 0x00),                // 发送缓冲区（15字节）
      running_(false),                // 线程运行标志
      mock_cycle_idx_(0) {            // 循环模式索引
    send_[0] = FRAME_HEADER;          // 预设帧头
    // 循环测试模式：依次切换所有工作模式
    mock_cycle_units_ = {MODE_COLOR, MODE_RING, MODE_DOCK, MODE_QR, MODE_IDLE};
}

/**
 * @brief 析构函数，确保资源正确释放
 * 
 * @details 自动调用 close() 方法，停止后台线程并关闭串口。
 */
SerialComm::~SerialComm() {
    close();
}

/**
 * @brief 打开并配置串口
 * 
 * @details 使用 POSIX termios 接口打开串口设备，配置为 8N1 模式。
 *          采用两阶段打开策略：先以非阻塞方式打开，再清除非阻塞标志。
 * 
 * @return bool 操作是否成功
 *         - true: 串口打开成功
 *         - false: 串口打开失败
 * 
 * @note 串口配置：
 *       - 8N1 模式：8位数据位、无校验、1位停止位
 *       - 波特率：根据 baudrate_ 参数设置
 *       - 原始输入模式：禁用行缓冲、回显、信号处理
 *       - 读取超时：VMIN=0, VTIME=1（约100ms超时）
 *       
 * @note 两阶段打开策略：
 *       1. 以非阻塞方式打开（O_NONBLOCK），避免某些驱动在 DCD 信号未就绪时阻塞
 *       2. 清除非阻塞标志，改用 VMIN/VTIME 控制读取超时
 *       
 * @note 平台差异：
 *       - Linux: 设备路径为 /dev/ttyUSB0、/dev/ttyACM0 等
 *       - Windows: 设备路径为 COM1、COM2 等（需使用 Windows API）
 *       
 * @see baudToSpeed(), close()
 */
bool SerialComm::open() {
    // 步骤 1: 以非阻塞方式打开，避免某些驱动在 DCD 信号未就绪时阻塞
    fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "串口打开失败: " << std::strerror(errno)
                  << " (" << port_ << ")" << std::endl;
        return false;
    }

    // 步骤 2: 清除非阻塞标志，改用 VMIN/VTIME 控制读取超时
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags != -1) {
        ::fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);
    }

    // 步骤 3: 获取当前串口配置
    struct termios tty;
    std::memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "tcgetattr 失败: " << std::strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // 步骤 4: 设置波特率
    speed_t speed = baudToSpeed(baudrate_);
    cfsetospeed(&tty, speed);  // 输出波特率
    cfsetispeed(&tty, speed);  // 输入波特率

    // 步骤 5: 配置 8N1 模式（8位数据位、无校验、1位停止位）
    // 注意：POSIX termios 常量为 int，字段为 tcflag_t(unsigned)，需显式转换避免符号转换警告
    tty.c_cflag &= static_cast<tcflag_t>(~(PARENB | CSTOPB | CSIZE));  // 清除校验位、停止位、数据位设置
    tty.c_cflag |= static_cast<tcflag_t>(CS8 | CLOCAL | CREAD);        // 8位数据位、忽略调制解调器状态、启用接收

    // 步骤 6: 配置原始输入模式（禁用所有特殊处理）
    tty.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ECHOE | ISIG));  // 禁用规范模式、回显、信号
    tty.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF | IXANY | IGNBRK | BRKINT |
                                           PARMRK | ISTRIP | INLCR | IGNCR | ICRNL));  // 禁用软件流控和特殊字符处理
    tty.c_oflag &= static_cast<tcflag_t>(~OPOST);  // 禁用输出处理

    // 步骤 7: 配置读取超时（VMIN=0, VTIME=1，约100ms超时）
    tty.c_cc[VMIN] = 0;   // 最少读取字节数
    tty.c_cc[VTIME] = 1;  // 超时时间（单位：100ms）

    // 步骤 8: 应用配置
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "tcsetattr 失败: " << std::strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // 步骤 9: 清空输入缓冲区
    tcflush(fd_, TCIFLUSH);
    
    std::cout << "串口已打开: " << port_ << " @ " << baudrate_ << std::endl;
    return true;
}

/**
 * @brief 关闭串口并停止后台线程
 * 
 * @details 线程安全地释放资源：
 *          1. 设置运行标志为 false，通知线程退出
 *          2. 等待线程完全退出（join）
 *          3. 关闭串口文件描述符
 * 
 * @note 线程退出流程：
 *       - running_ = false 后，线程会在下一次循环检查时退出
 *       - join() 确保线程完全退出后才继续执行
 *       
 * @see open(), startThread()
 */
void SerialComm::close() {
    // 步骤 1: 停止后台线程
    running_ = false;
    
    // 步骤 2: 等待线程退出
    if (thread_.joinable()) {
        thread_.join();
    }
    
    // 步骤 3: 关闭串口文件描述符
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

/**
 * @brief 启动串口通信
 * 
 * @details 根据配置决定启动真实串口还是模拟模式。
 *          如果真实串口打开失败，自动回退到模拟模式。
 * 
 * @note 启动流程：
 *       1. 如果 mock_=true，直接启动模拟线程
 *       2. 如果 mock_=false，尝试打开真实串口
 *       3. 如果真实串口打开失败，自动切换到模拟模式
 *       
 * @see open(), startThread()
 */
void SerialComm::start() {
    // 模拟模式：直接启动线程
    if (mock_) {
        startThread();
        return;
    }
    
    // 真实串口模式：尝试打开串口
    if (open()) {
        startThread();
    } else {
        // 串口打开失败，回退到模拟模式
        std::cout << "[警告] 真实串口打开失败，回退到模拟模式" << std::endl;
        mock_ = true;
        startThread();
    }
}

/**
 * @brief 启动后台线程
 * 
 * @details 根据模式启动不同的处理函数：
 *          - 模拟模式：processMock()
 *          - 真实串口：processReal()
 * 
 * @see processReal(), processMock()
 */
void SerialComm::startThread() {
    running_ = true;  // 设置运行标志
    
    if (mock_) {
        // 模拟模式：启动模拟处理线程
        thread_ = std::thread(&SerialComm::processMock, this);
    } else {
        // 真实串口模式：启动串口读取线程
        thread_ = std::thread(&SerialComm::processReal, this);
    }
}

/**
 * @brief 真实串口读取线程函数
 * 
 * @details 在后台持续读取串口数据，解析帧结构并更新工作模式。
 *          采用自旋锁机制，避免阻塞主线程。
 * 
 * @note 帧结构（4字节）：
 *       - 帧头(1): 0xAA
 *       - 工作模式(1): MODE_COLOR/MODE_RING/MODE_DOCK/MODE_QR
 *       - 目标数据(1): 目标编号或其他数据
 *       - 保留(1): 预留字段
 *       
 * @note 线程安全：
 *       - 使用 std::lock_guard 保护共享数据 receive_
 *       - 使用 std::atomic 存储工作模式，避免锁竞争
 *       
 * @note 错误处理：
 *       - 读取失败时输出错误信息并休眠 10ms，避免CPU占用过高
 */
void SerialComm::processReal() {
    uint8_t buf[4];
    
    while (running_) {
        // 从串口读取数据
        ssize_t n = ::read(fd_, buf, sizeof(buf));
        
        if (n > 0) {
            // 成功读取数据，解析帧结构
            std::vector<uint8_t> com_input;
            com_input.reserve(4);
            
            // 复制读取的数据（最多4字节）
            for (ssize_t i = 0; i < n && i < 4; ++i) {
                com_input.push_back(buf[i]);
            }
            
            // 补齐到4字节（不足部分填充0）
            while (com_input.size() < 4) {
                com_input.push_back(0);
            }
            
            // 加锁更新接收缓冲区
            {
                std::lock_guard<std::mutex> lock(mutex_);
                receive_ = com_input;
                
                // 验证帧头并更新工作模式
                if (receive_[0] == FRAME_HEADER) {
                    unit.store(receive_[1], std::memory_order_relaxed);      // 更新工作模式
                    unit_target.store(receive_[2], std::memory_order_relaxed);  // 更新目标数据
                }
            }
        } else if (n < 0) {
            // 读取异常，输出错误信息
            std::cerr << "串口读取异常: " << std::strerror(errno) << std::endl;
            // 休眠10ms，避免CPU占用过高
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

/**
 * @brief 模拟模式处理线程函数
 * 
 * @details 在后台定期更新工作模式，用于无串口设备的测试场景。
 *          支持固定模式和循环测试模式。
 * 
 * @note 循环测试模式：
 *       每100ms切换一次工作模式：COLOR → RING → DOCK → QR → IDLE → ...
 *       用于测试所有工作流程。
 *       
 * @note 固定模式：
 *       始终保持 mock_unit_ 指定的模式，用于测试特定流程。
 */
void SerialComm::processMock() {
    while (running_) {
        // 休眠100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (mock_cycle_) {
            // 循环测试模式：依次切换所有工作模式
            uint8_t u = mock_cycle_units_[mock_cycle_idx_];
            mock_cycle_idx_ = (mock_cycle_idx_ + 1) % mock_cycle_units_.size();
            unit.store(u, std::memory_order_relaxed);
        } else {
            // 固定模式：保持默认模式
            unit.store(mock_unit_, std::memory_order_relaxed);
        }
    }
}

/**
 * @brief 构建发送帧
 * 
 * @details 根据命令和数据构建完整的15字节帧结构。
 *          帧结构：帧头(1) + 命令(1) + 数据(12) + 校验和(1) + 帧尾(1)。
 * 
 * @param cmd 命令字节（CMD_COLOR/CMD_RING/CMD_DOCK/CMD_QR）
 * @param data_bytes 数据字节（最多12字节）
 * 
 * @note 帧结构详解：
 *       - [0]: 帧头 = 0xAA
 *       - [1]: 命令 = CMD_COLOR/CMD_RING/CMD_DOCK/CMD_QR
 *       - [2-13]: 数据（12字节）
 *       - [14]: 校验和 = 命令 + 数据的累加和低8位
 *       - [15]: 帧尾 = 0x55
 *       
 * @note 校验和计算：
 *       校验和 = sum(send_[1:14]) & 0xFF
 *       即命令字节 + 数据字节的累加和，取低8位
 *       
 * @see sendCoordinates(), sendQrData()
 */
void SerialComm::buildFrame(uint8_t cmd, const std::vector<uint8_t>& data_bytes) {
    // 步骤 1: 初始化发送缓冲区（15字节，全0）
    send_.assign(15, 0x00);
    
    // 步骤 2: 设置帧头和命令
    send_[0] = FRAME_HEADER;  // 帧头 = 0xAA
    send_[1] = cmd;           // 命令字节
    
    // 步骤 3: 复制数据（最多12字节）
    for (std::size_t i = 0; i < FRAME_DATA_LEN && i < data_bytes.size(); ++i) {
        send_[2 + i] = data_bytes[i];
    }
    
    // 步骤 4: 计算校验和 = 命令 + 数据的累加和低8位
    uint8_t checksum = 0;
    for (std::size_t i = 1; i < FRAME_CHECKSUM_IDX; ++i) {
        checksum = static_cast<uint8_t>(checksum + send_[i]);
    }
    
    // 步骤 5: 设置校验和和帧尾
    send_[FRAME_CHECKSUM_IDX] = checksum;  // 校验和
    send_[FRAME_TAIL_IDX] = FRAME_TAIL;   // 帧尾 = 0x55
}

/**
 * @brief 发送坐标数据
 * 
 * @details 根据不同的命令类型，将坐标数据编码为12字节数据帧并发送。
 *          支持三色物料坐标、圆环坐标、停靠点坐标等多种格式。
 * 
 * @param cmd 命令类型（CMD_COLOR/CMD_RING/CMD_DOCK）
 * @param coords 坐标列表（每个坐标为 <x, y> 键值对）
 * 
 * @note CMD_COLOR 格式（三色物料中心点）：
 *       - 数据格式：[X1_H, X1_L, Y1_H, Y1_L, X2_H, X2_L, Y2_H, Y2_L, X3_H, X3_L, Y3_H, Y3_L]
 *       - 编码方式：每个坐标占用4字节（2字节X + 2字节Y），共3个坐标
 *       - 字节序：大端序（高字节在前）
 *       
 * @note CMD_RING/CMD_DOCK 格式（中间坐标 + Y差值）：
 *       - 数据格式：[X2_H, X2_L, Y2_H, Y2_L, ΔY_H, ΔY_L, ...]
 *       - 编码方式：中间点坐标(X2, Y2) + Y轴差值(Y1-Y3)
 *       - 用途：用于姿态检测，判断机器人是否对准目标
 *       
 * @note 线程安全：
 *       使用 std::lock_guard 保护帧构建过程，避免多线程竞争
 *       
 * @see buildFrame(), packWord()
 */
void SerialComm::sendCoordinates(uint8_t cmd,
                                 const std::vector<std::pair<int, int>>& coords) {
    // 初始化数据缓冲区（12字节，全0）
    std::vector<uint8_t> data(FRAME_DATA_LEN, 0x00);

    // 根据命令类型编码数据
    if (cmd == CMD_COLOR) {
        // 三色物料中心点编码（每个坐标 2字节x + 2字节y，共12字节）
        if (coords.size() >= 3) {
            for (std::size_t i = 0; i < 3; ++i) {
                // 编码X坐标（高字节 + 低字节）
                auto xh = packWord(coords[i].first);
                // 编码Y坐标（高字节 + 低字节）
                auto yh = packWord(coords[i].second);
                
                // 填充数据：[X_H, X_L, Y_H, Y_L]
                data[i * 4 + 0] = xh.first;   // X高字节
                data[i * 4 + 1] = xh.second;  // X低字节
                data[i * 4 + 2] = yh.first;   // Y高字节
                data[i * 4 + 3] = yh.second;  // Y低字节
            }
        }
    } else if (cmd == CMD_RING || cmd == CMD_DOCK) {
        // 中间坐标 + Y差值编码
        if (coords.size() >= 3) {
            // 提取中间点坐标（索引1）
            int x2 = coords[1].first;
            int y2 = coords[1].second;
            // 计算Y轴差值（左侧Y - 右侧Y）
            int y1 = coords[0].second;  // 左侧Y
            int y3 = coords[2].second;  // 右侧Y
            
            // 编码中间点X坐标
            auto p = packWord(x2);
            data[0] = p.first;   // X高字节
            data[1] = p.second;  // X低字节
            
            // 编码中间点Y坐标
            p = packWord(y2);
            data[2] = p.first;   // Y高字节
            data[3] = p.second;  // Y低字节
            
            // 编码Y轴差值（用于姿态判断）
            p = packWord(y1 - y3);
            data[4] = p.first;   // ΔY高字节
            data[5] = p.second;  // ΔY低字节
        }
    }

    // 构建帧并发送（线程安全）
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        buildFrame(cmd, data);  // 构建帧
        frame = send_;          // 复制帧数据
    }
    
    // 发送帧
    transmit(frame);
}

/**
 * @brief 发送二维码数据
 * 
 * @details 将二维码字符串编码为数据帧并发送。
 *          字符串长度限制为12字节（FRAME_DATA_LEN）。
 * 
 * @param qr_data 二维码字符串（如 "A1B2C3"）
 * 
 * @note 编码方式：
 *       - 直接将字符串的ASCII码复制到数据区
 *       - 长度超过12字节时截断
 *       - 示例："A1B2C3" → [0x41, 0x31, 0x42, 0x32, 0x43, 0x33, 0x00, ...]
 *       
 * @see buildFrame()
 */
void SerialComm::sendQrData(const std::string& qr_data) {
    // 初始化数据缓冲区（12字节，全0）
    std::vector<uint8_t> data(FRAME_DATA_LEN, 0x00);
    
    // 确定字符串长度（最多12字节）
    std::size_t len = qr_data.size();
    if (len > FRAME_DATA_LEN) {
        len = FRAME_DATA_LEN;  // 截断到12字节
    }
    
    // 复制字符串到数据区
    for (std::size_t i = 0; i < len; ++i) {
        data[i] = static_cast<uint8_t>(qr_data[i]);
    }

    // 构建帧并发送（线程安全）
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        buildFrame(CMD_QR, data);  // 构建二维码帧
        frame = send_;             // 复制帧数据
    }
    
    // 发送帧
    transmit(frame);
}

/**
 * @brief 发送帧数据
 * 
 * @details 根据模式选择发送方式：
 *          - 真实串口模式：通过 write() 系统调用发送
 *          - 模拟模式：输出帧内容到控制台
 * 
 * @param frame 完整的15字节帧数据
 * 
 * @note 发送流程：
 *       1. 检查模式（真实/模拟）
 *       2. 真实模式：调用 ::write() 发送数据
 *       3. 模拟模式：输出帧内容到控制台
 *       
 * @note 错误处理：
 *       发送失败时输出错误信息，但不中断程序运行
 */
void SerialComm::transmit(const std::vector<uint8_t>& frame) {
    if (!mock_ && fd_ >= 0) {
        // 真实串口模式：发送数据
        ssize_t n = ::write(fd_, frame.data(), frame.size());
        if (n < 0) {
            std::cerr << "串口发送失败: " << std::strerror(errno) << std::endl;
        }
    } else {
        // 模拟模式：输出帧内容
        std::cout << "[模拟] 发送数据: [";
        for (std::size_t i = 0; i < frame.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << static_cast<int>(frame[i]);
        }
        std::cout << "]" << std::endl;
    }
}
