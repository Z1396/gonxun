// 串口通信模块实现
// 使用 POSIX termios 进行串口通信，std::thread 进行后台接收
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

// 将16位有符号整数拆分为高8位、低8位字节
std::pair<uint8_t, uint8_t> packWord(int value) {
    uint16_t v = static_cast<uint16_t>(value);
    return {static_cast<uint8_t>((v & 0xff00) >> 8),
            static_cast<uint8_t>(v & 0xff)};
}

// 波特率转 termios 速度常量
speed_t baudToSpeed(int baud) {
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
      mock_cycle_idx_(0) {
    send_[0] = FRAME_HEADER;
    mock_cycle_units_ = {MODE_COLOR, MODE_RING, MODE_DOCK, MODE_QR, MODE_IDLE};
}

SerialComm::~SerialComm() {
    close();
}

bool SerialComm::open() {
    // 以非阻塞方式打开，避免某些驱动在 DCD 信号未就绪时阻塞
    fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "串口打开失败: " << std::strerror(errno)
                  << " (" << port_ << ")" << std::endl;
        return false;
    }

    // 清除非阻塞标志，改用 VMIN/VTIME 控制读取超时
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags != -1) {
        ::fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);
    }

    struct termios tty;
    std::memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "tcgetattr 失败: " << std::strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // 设置波特率
    speed_t speed = baudToSpeed(baudrate_);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // 8N1 配置：8位数据位、无校验、1位停止位
    // 注意：POSIX termios 常量为 int，字段为 tcflag_t(unsigned)，需显式转换避免符号转换警告
    tty.c_cflag &= static_cast<tcflag_t>(~(PARENB | CSTOPB | CSIZE));
    tty.c_cflag |= static_cast<tcflag_t>(CS8 | CLOCAL | CREAD);

    // 原始输入模式
    tty.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ECHOE | ISIG));
    tty.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF | IXANY | IGNBRK | BRKINT |
                                           PARMRK | ISTRIP | INLCR | IGNCR | ICRNL));
    tty.c_oflag &= static_cast<tcflag_t>(~OPOST);

    // 读取超时：VMIN=0, VTIME=1 (约100ms，对应 Python timeout=0.05 量级)
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

void SerialComm::start() {
    if (mock_) {
        startThread();
        return;
    }
    if (open()) {
        startThread();
    } else {
        std::cout << "[警告] 真实串口打开失败，回退到模拟模式" << std::endl;
        mock_ = true;
        startThread();
    }
}

void SerialComm::startThread() {
    running_ = true;
    if (mock_) {
        thread_ = std::thread(&SerialComm::processMock, this);
    } else {
        thread_ = std::thread(&SerialComm::processReal, this);
    }
}

void SerialComm::processReal() {
    uint8_t buf[4];
    while (running_) {
        ssize_t n = ::read(fd_, buf, sizeof(buf));
        if (n > 0) {
            std::vector<uint8_t> com_input;
            com_input.reserve(4);
            for (ssize_t i = 0; i < n && i < 4; ++i) {
                com_input.push_back(buf[i]);
            }
            while (com_input.size() < 4) {
                com_input.push_back(0);
            }
            std::lock_guard<std::mutex> lock(mutex_);
            receive_ = com_input;
            if (receive_[0] == FRAME_HEADER) {
                unit.store(receive_[1], std::memory_order_relaxed);
                unit_target.store(receive_[2], std::memory_order_relaxed);
            }
        } else if (n < 0) {
            std::cerr << "串口读取异常: " << std::strerror(errno) << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void SerialComm::processMock() {
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

void SerialComm::buildFrame(uint8_t cmd, const std::vector<uint8_t>& data_bytes) {
    // 帧结构：帧头(1) + 命令(1) + 数据(12) + 校验和(1) + 帧尾(1) = 15字节
    send_.assign(15, 0x00);
    send_[0] = FRAME_HEADER;
    send_[1] = cmd;
    for (std::size_t i = 0; i < FRAME_DATA_LEN && i < data_bytes.size(); ++i) {
        send_[2 + i] = data_bytes[i];
    }
    // 校验和 = 命令 + 数据(索引1到12) 的累加和低8位
    uint8_t checksum = 0;
    for (std::size_t i = 1; i < FRAME_CHECKSUM_IDX; ++i) {
        checksum = static_cast<uint8_t>(checksum + send_[i]);
    }
    send_[FRAME_CHECKSUM_IDX] = checksum;
    send_[FRAME_TAIL_IDX] = FRAME_TAIL;
}

void SerialComm::sendCoordinates(uint8_t cmd,
                                 const std::vector<std::pair<int, int>>& coords) {
    std::vector<uint8_t> data(FRAME_DATA_LEN, 0x00);

    if (cmd == CMD_COLOR) {
        // 三色物料中心点 (每个坐标 2字节x + 2字节y，共12字节)
        if (coords.size() >= 3) {
            for (std::size_t i = 0; i < 3; ++i) {
                auto xh = packWord(coords[i].first);
                auto yh = packWord(coords[i].second);
                data[i * 4 + 0] = xh.first;
                data[i * 4 + 1] = xh.second;
                data[i * 4 + 2] = yh.first;
                data[i * 4 + 3] = yh.second;
            }
        }
    } else if (cmd == CMD_RING || cmd == CMD_DOCK) {
        // 中间坐标 + Y差值
        if (coords.size() >= 3) {
            int x2 = coords[1].first;
            int y2 = coords[1].second;
            int y1 = coords[0].second;
            int y3 = coords[2].second;
            auto p = packWord(x2);
            data[0] = p.first;
            data[1] = p.second;
            p = packWord(y2);
            data[2] = p.first;
            data[3] = p.second;
            p = packWord(y1 - y3);
            data[4] = p.first;
            data[5] = p.second;
        }
    }

    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        buildFrame(cmd, data);
        frame = send_;
    }
    transmit(frame);
}

void SerialComm::sendQrData(const std::string& qr_data) {
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
        buildFrame(CMD_QR, data);
        frame = send_;
    }
    transmit(frame);
}

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
