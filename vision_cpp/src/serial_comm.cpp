/**
 * @file serial_comm.cpp
 * @brief 串口通信模块实现。
 *
 * 真实模式使用 POSIX termios 8N1，6 字节帧状态机同步解析。
 * 模拟模式启动 500ms 后发 match_start=1，后续根据发送的 move/grab 帧
 * 按预设延迟（move: 300ms+steps*50ms，grab: 800ms）产生对应 done 信号。
 */
#include "serial_comm.hpp"

#include <array>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace {

/// @brief 波特率数值转 termios 速度常量
/// @param baud 波特率数值
/// @return 对应的 speed_t；不支持时默认 B115200
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

/// @brief 获取当前时间戳（毫秒）
/// @return 自 epoch 起的毫秒数
int64_t now_ms() noexcept {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

SerialComm::SerialComm(bool mock, const std::string& port, int baudrate) noexcept
    : mock_(mock),
      port_(port),
      baudrate_(baudrate),
      fd_(-1),
      running_(false) {}

SerialComm::~SerialComm() {
    close();
}

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

    speed_t speed = baud_to_speed(baudrate_);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // 8N1
    tty.c_cflag &= static_cast<tcflag_t>(~(PARENB | CSTOPB | CSIZE));
    tty.c_cflag |= static_cast<tcflag_t>(CS8 | CLOCAL | CREAD);
    tty.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ECHOE | ISIG));
    tty.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF | IXANY | IGNBRK | BRKINT |
                                           PARMRK | ISTRIP | INLCR | IGNCR | ICRNL));
    tty.c_oflag &= static_cast<tcflag_t>(~OPOST);
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

void SerialComm::start_thread() {
    running_ = true;
    if (mock_) {
        thread_ = std::thread(&SerialComm::process_mock, this);
    } else {
        thread_ = std::thread(&SerialComm::process_real, this);
    }
}

// ==== 发送接口 ====

void SerialComm::send_move_frame(uint16_t angle, int16_t steps) {
    auto frame = gonxun::build_move_frame(angle, steps);
    transmit(frame.to_bytes());
    if (mock_) record_mock_send(static_cast<uint8_t>(gonxun::FrameMode::Path),
                                static_cast<uint8_t>(gonxun::GrabAction::None));
    // 模拟延迟：steps 为步进电机步数，每格 480 步，每格延迟 50ms
    if (mock_) {
        int grid_steps = std::abs(steps) / 480;  // 步进电机步数→格子数
        int delay_ms = 300 + grid_steps * 50;
        mock_move_done_at_.store(now_ms() + delay_ms, std::memory_order_relaxed);
        mock_move_pending_.store(true, std::memory_order_relaxed);
    }
}

void SerialComm::send_locate_frame(uint16_t x, uint16_t y, uint8_t grab) {
    auto frame = gonxun::build_locate_frame(x, y, grab);
    transmit(frame.to_bytes());
    if (mock_ && grab == 1) {
        mock_grab_done_at_.store(now_ms() + 800, std::memory_order_relaxed);
        mock_grab_pending_.store(true, std::memory_order_relaxed);
    }
}

void SerialComm::send_grab_frame() {
    auto frame = gonxun::build_grab_frame();
    transmit(frame.to_bytes());
    if (mock_) {
        mock_grab_done_at_.store(now_ms() + 800, std::memory_order_relaxed);
        mock_grab_pending_.store(true, std::memory_order_relaxed);
    }
}

void SerialComm::send_raw_frame(const std::vector<uint8_t>& frame) {
    transmit(frame);
}

void SerialComm::transmit(const std::vector<uint8_t>& frame) {
    std::lock_guard<std::mutex> lock(tx_mutex_);
    if (!mock_ && fd_ >= 0) {
        ssize_t n = ::write(fd_, frame.data(), frame.size());
        if (n < 0) {
            std::cerr << "串口发送失败: " << std::strerror(errno) << std::endl;
        }
    } else {
        std::cout << "[模拟发送] ";
        for (auto b : frame) {
            std::cout << std::hex << static_cast<int>(b) << " ";
        }
        std::cout << std::dec << std::endl;
    }
}

void SerialComm::record_mock_send(uint8_t /*mode*/, uint8_t /*grab*/) noexcept {
    // 兼容预留：用于扩展其他记录逻辑
}

// ==== 接收处理 ====

void SerialComm::dispatch_feedback(const gonxun::FeedbackFrame& fb) noexcept {
    // match_start 锁存：仅在首次收到 match_start=1 时触发回调
    if (fb.match_start == 1 && !match_started_) {
        match_started_ = true;
        std::lock_guard<std::mutex> lock(cb_mutex_);
        if (match_start_cb_) match_start_cb_(true);
    }

    // move_done：每次为 1 时触发
    if (fb.move_done == 1) {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        if (move_done_cb_) move_done_cb_();
    }

    // grab_done：每次为 1 时触发
    if (fb.grab_done == 1) {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        if (grab_done_cb_) grab_done_cb_();
    }
}

void SerialComm::process_real() {
    uint8_t buf[64];
    while (running_) {
        ssize_t n = ::read(fd_, buf, sizeof(buf));
        if (n > 0) {
            std::lock_guard<std::mutex> lock(rx_mutex_);
            for (ssize_t i = 0; i < n; ++i) {
                rx_buf_.push_back(buf[i]);
            }
            // 帧同步：扫描 0x66 头，凑够 6 字节尝试解析
            while (rx_buf_.size() >= gonxun::FB_FRAME_LEN) {
                if (rx_buf_.front() != gonxun::FRAME_HEADER) {
                    rx_buf_.pop_front();
                    continue;
                }
                // 取 6 字节尝试解析
                std::array<uint8_t, gonxun::FB_FRAME_LEN> tmp;
                std::copy(rx_buf_.begin(), rx_buf_.begin() + gonxun::FB_FRAME_LEN, tmp.begin());
                auto fb = gonxun::parse_feedback(tmp.data(), tmp.size());
                if (fb) {
                    dispatch_feedback(*fb);
                    // 消费已解析的 6 字节
                    for (std::size_t i = 0; i < gonxun::FB_FRAME_LEN; ++i) {
                        rx_buf_.pop_front();
                    }
                } else {
                    // 校验失败：丢弃首字节继续扫描（resync）
                    rx_buf_.pop_front();
                }
            }
        } else if (n < 0) {
            std::cerr << "串口读取异常: " << std::strerror(errno) << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void SerialComm::process_mock() {
    // 启动 500ms 后发 match_start=1（锁存，仅触发一次）
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (!mock_match_sent_.exchange(true)) {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        if (match_start_cb_) match_start_cb_(true);
        match_started_ = true;
        std::cout << "[模拟] match_start=1" << std::endl;
    }

    // 持续轮询：检查是否有待触发的 move_done / grab_done
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int64_t now = now_ms();

        if (mock_move_pending_.load(std::memory_order_relaxed) &&
            now >= mock_move_done_at_.load(std::memory_order_relaxed)) {
            mock_move_pending_.store(false, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(cb_mutex_);
            if (move_done_cb_) move_done_cb_();
            std::cout << "[模拟] move_done=1" << std::endl;
        }

        if (mock_grab_pending_.load(std::memory_order_relaxed) &&
            now >= mock_grab_done_at_.load(std::memory_order_relaxed)) {
            mock_grab_pending_.store(false, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(cb_mutex_);
            if (grab_done_cb_) grab_done_cb_();
            std::cout << "[模拟] grab_done=1" << std::endl;
        }
    }
}
