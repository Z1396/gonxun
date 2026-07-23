/**
 * @file serial_comm.cpp
 * @brief 串口通信模块实现
 *
 * 实现串口打开/配置/收发及模拟模式。真实模式使用 POSIX termios 接口，
 * 8N1 格式，非阻塞打开后切换为阻塞读取。模拟模式周期切换工作模式，
 * 发送时仅打印帧内容。
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

namespace {

/**
 * @brief 将16位有符号整数拆分为高8位、低8位字节（大端序）
 * @param value 待编码的整数值
 * @return (高字节, 低字节)
 */
std::pair<uint8_t, uint8_t> pack_word(int value) noexcept {
    uint16_t v = static_cast<uint16_t>(value);
    return {static_cast<uint8_t>((v & 0xff00) >> 8),
            static_cast<uint8_t>(v & 0xff)};
}

/**
 * @brief 波特率数值转 termios 速度常量
 * @param baud 波特率数值
 * @return 对应的 termios speed_t；不支持的波特率默认返回 B115200
 */
speed_t baud_to_speed(int baud) noexcept {
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 921600: return B921600;
        default:     return B115200;
    }
}

}  // namespace

/**
 * @brief 构造函数，初始化缓冲区和模拟参数
 * @note 发送缓冲区预填帧头，模拟周期序列包含全部5种工作模式
 */
SerialComm::SerialComm(bool mock, const std::string& port, int baudrate,
                       uint8_t mock_unit, bool mock_cycle)
    : mock_(mock),
      port_(port),
      baudrate_(baudrate),
      mock_unit_(mock_unit),
      mock_cycle_(mock_cycle),
      fd_(-1),
      receive_(4, 0),
      send_(15, 0x00),
      running_(false),
      mock_cycle_idx_(0) 
{
    send_[0] = FRAME_HEADER;
    mock_cycle_units_ = {MODE_COLOR, MODE_RING, MODE_DOCK, MODE_QR, MODE_IDLE};
}

/** @brief 析构，确保串口关闭 */
SerialComm::~SerialComm() {
    close();
}

/**
 * @brief 打开并配置真实串口
 * @return 成功返回 true
 * @note 配置 8N1 (8数据位/无校验/1停止位)，禁用流控和回显，
 *       VMIN=0/VTIME=1 实现带超时的阻塞读取
 */
bool SerialComm::open() {
    fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "串口打开失败: " << std::strerror(errno)
                  << " (" << port_ << ")" << std::endl;
        return false;
    }

    // 打开后切换为阻塞模式
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags != -1) {
        ::fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);
    }

    struct termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "tcgetattr 失败: " << std::strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // 设置波特率
    speed_t speed = baud_to_speed(baudrate_);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // 8N1: 8数据位, 无校验, 1停止位
    tty.c_cflag &= static_cast<tcflag_t>(~(PARENB | CSTOPB | CSIZE));
    tty.c_cflag |= static_cast<tcflag_t>(CS8 | CLOCAL | CREAD);

    // 禁用回显和规范模式
    tty.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ECHOE | ISIG));
    // 禁用软件流控和特殊字符处理
    tty.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF | IXANY | IGNBRK | BRKINT |
                                           PARMRK | ISTRIP | INLCR | IGNCR | ICRNL));
    tty.c_oflag &= static_cast<tcflag_t>(~OPOST);

    // VMIN=0, VTIME=1: 读取超时 0.1秒
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "tcsetattr 失败: " << std::strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    tcflush(fd_, TCIFLUSH);

    std::cout << "串口已打开: " << port_ << " @ " << baudrate_ << std::endl;
    return true;
}

/** @brief 停止接收线程并关闭串口文件描述符 */
void SerialComm::close() {
    running_ = false;

    if (thread_.joinable()) {
        thread_.join();
    }

    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

/**
 * @brief 启动串口通信（含接收线程）
 * @note 真实串口打开失败时自动回退到模拟模式
 */
void SerialComm::start() {
    if (mock_) {
        start_thread();
        return;
    }

    if (open()) {
        start_thread();
    } else {
        std::cout << "[警告] 真实串口打开失败，回退到模拟模式" << std::endl;
        mock_ = true;
        start_thread();
    }
}

/** @brief 根据当前模式启动对应接收线程 */
void SerialComm::start_thread() {
    running_ = true;

    if (mock_) {
        thread_ = std::thread(&SerialComm::process_mock, this);
    } else {
        thread_ = std::thread(&SerialComm::process_real, this);
    }
}

/**
 * @brief 真实串口接收循环
 * @note 持续读取最多4字节，若帧头为 0x66 则解析 [1]=unit, [2]=unit_target
 */
void SerialComm::process_real() {
    uint8_t buf[4];

    while (running_) {
        ssize_t n = ::read(fd_, buf, sizeof(buf));

        if (n > 0) {
            std::vector<uint8_t> com_input;
            com_input.reserve(4);

            for (ssize_t i = 0; i < n && i < 4; ++i) {
                com_input.push_back(buf[i]);
            }

            // 不足4字节补零
            while (com_input.size() < 4) {
                com_input.push_back(0);
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                receive_ = com_input;

                // 帧头校验：[0]=0x66, [1]=工作模式, [2]=目标编号
                if (receive_[0] == FRAME_HEADER) {
                    unit.store(receive_[1], std::memory_order_relaxed);
                    unit_target.store(receive_[2], std::memory_order_relaxed);
                }
            }
        } else if (n < 0) {
            std::cerr << "串口读取异常: " << std::strerror(errno) << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

/**
 * @brief 模拟串口接收循环
 * @note 周期模式: 每100ms切换 mock_cycle_units_ 中的下一个模式
 *       固定模式: 始终设置 mock_unit_
 */
void SerialComm::process_mock() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (mock_cycle_) {
            uint8_t u = mock_cycle_units_[mock_cycle_idx_];
            mock_cycle_idx_ = (mock_cycle_idx_ + 1) % mock_cycle_units_.size();
            unit.store(u, std::memory_order_relaxed);
        } else {
            unit.store(mock_unit_, std::memory_order_relaxed);
        }
    }
}

/**
 * @brief 构建发送帧
 * @param cmd 命令字节
 * @param data_bytes 数据域字节（最多12字节）
 * @note 帧格式: [HEADER][CMD][DATA×12][CHECKSUM][TAIL]
 *       校验和 = (cmd + data[0..11]) 累加和 & 0xFF
 */
void SerialComm::build_frame(uint8_t cmd, const std::vector<uint8_t>& data_bytes) {
    send_.assign(15, 0x00);

    send_[0] = FRAME_HEADER;
    send_[1] = cmd;

    for (std::size_t i = 0; i < FRAME_DATA_LEN && i < data_bytes.size(); ++i) {
        send_[2 + i] = data_bytes[i];
    }

    // 计算校验和: cmd + data 累加
    uint8_t checksum = 0;
    for (std::size_t i = 1; i < FRAME_CHECKSUM_IDX; ++i) {
        checksum = static_cast<uint8_t>(checksum + send_[i]);
    }

    send_[FRAME_CHECKSUM_IDX] = checksum;
    send_[FRAME_TAIL_IDX] = FRAME_TAIL;
}

/**
 * @brief 发送坐标数据帧
 * @param cmd 命令字节
 * @param coords 坐标列表
 * @note CMD_COLOR: 3组(x,y)共12字节，每组4字节(x高+x低+y高+y低)
 *       CMD_RING/CMD_DOCK: 6字节 = (x2高,x2低,y2高,y2低,y1-y3高,y1-y3低)
 */
void SerialComm::send_coordinates(uint8_t cmd,
                                  const std::vector<std::pair<int, int>>& coords) {
    std::vector<uint8_t> data(FRAME_DATA_LEN, 0x00);

    if (cmd == CMD_COLOR) {
        // 颜色模式: 编码3组坐标 (x,y) 各16位大端
        if (coords.size() >= 3) {
            for (std::size_t i = 0; i < 3; ++i) {
                auto xh = pack_word(coords[i].first);
                auto yh = pack_word(coords[i].second);

                data[i * 4 + 0] = xh.first;
                data[i * 4 + 1] = xh.second;
                data[i * 4 + 2] = yh.first;
                data[i * 4 + 3] = yh.second;
            }
        }
    } else if (cmd == CMD_RING || cmd == CMD_DOCK) {
        // 圆环/对接模式: 编码中间圆心(x2,y2)和左右y差值(y1-y3)
        if (coords.size() >= 3) {
            int x2 = coords[1].first;
            int y2 = coords[1].second;
            int y1 = coords[0].second;
            int y3 = coords[2].second;

            auto p = pack_word(x2);
            data[0] = p.first;
            data[1] = p.second;

            p = pack_word(y2);
            data[2] = p.first;
            data[3] = p.second;

            p = pack_word(y1 - y3);
            data[4] = p.first;
            data[5] = p.second;
        }
    }

    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        build_frame(cmd, data);
        frame = send_;
    }

    transmit(frame);
}

/**
 * @brief 发送二维码数据帧
 * @param qr_data QR码字符串，逐字节写入数据域，截断至12字节
 */
void SerialComm::send_qr_data(const std::string& qr_data) {
    std::vector<uint8_t> data(FRAME_DATA_LEN, 0x00);

    std::size_t len = qr_data.size();
    if (len > FRAME_DATA_LEN) {
        len = FRAME_DATA_LEN;
    }

    for (std::size_t i = 0; i < len; ++i) {
        data[i] = static_cast<uint8_t>(qr_data[i]);
    }

    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        build_frame(CMD_QR, data);
        frame = send_;
    }

    transmit(frame);
}

/**
 * @brief 发送帧到串口或模拟输出
 * @param frame 完整帧字节数组
 * @note 模拟模式下打印帧的十进制内容到标准输出
 */
void SerialComm::transmit(const std::vector<uint8_t>& frame) {
    if (!mock_ && fd_ >= 0) {
        ssize_t n = ::write(fd_, frame.data(), frame.size());
        if (n < 0) {
            std::cerr << "串口发送失败: " << std::strerror(errno) << std::endl;
        }
    } else {
        std::cout << "[模拟] 发送数据: [";
        for (std::size_t i = 0; i < frame.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << static_cast<int>(frame[i]);
        }
        std::cout << "]" << std::endl;
    }
}

/**
 * @brief 直接发送原始帧数据
 * @param frame 完整帧字节数组
 */
void SerialComm::send_raw_frame(const std::vector<uint8_t>& frame) {
    transmit(frame);
}
