/**
 * @file serial_comm.hpp
 * @brief 串口通信模块，支持真实硬件串口和模拟模式。
 *
 * 负责上位机与下位机之间的统一串口通信：
 *  - 发送端按 12 字节 CommandFrame 协议打包指令（移动/定位/抓取）
 *  - 接收端解析 6 字节 FeedbackFrame，派发 match_start/move_done/grab_done 事件
 *  - 模拟模式下自动生成 done 信号以支持 stop-and-wait 队列机制调试
 *
 * 协议格式详见 motion_protocol.hpp。
 */
#pragma once

#include "motion_protocol.hpp"
#include "common_types.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace gonxun {

// ==== 串口状态（std::variant 表达互斥状态） ====

/// 真实串口状态
struct RealSerial {
    FileDescriptor fd;  ///< 文件描述符 RAII 持有者
    std::string port;   ///< 串口设备路径
    int baudrate;       ///< 波特率
};

/// 模拟串口状态
struct MockSerial {
    std::string port;   ///< 模拟设备路径（用于显示）
    int baudrate;       ///< 模拟波特率
};

/// 串口运行时状态
using SerialState = std::variant<std::monostate, RealSerial, MockSerial>;

// ==== 串口通信类 ====

/**
 * @brief 串口通信类，支持真实串口和模拟模式。
 *
 * 使用 std::variant 表达真实串口和模拟串口的互斥状态，
 * 避免多个 bool 标志拼状态机，符合 Talos 规范。
 */
class SerialComm {
public:
    /// @brief 比赛开始回调签名（参数为 true 表示收到 match_start=1）
    using MatchStartCallback = std::function<void(bool)>;
    /// @brief 走路完成回调签名
    using MoveDoneCallback = std::function<void()>;
    /// @brief 抓取完成回调签名
    using GrabDoneCallback = std::function<void()>;

    /// @brief 创建串口通信实例（模拟模式）
    /// @param port 模拟设备路径（用于显示）
    /// @param baudrate 模拟波特率
    /// @return 成功返回 SerialComm 指针，失败返回错误信息
    [[nodiscard]] static Expected<std::unique_ptr<SerialComm>> create_mock(
        const std::string& port = "/dev/ttyCH341USB0",
        int baudrate = 115200) noexcept;

    /// @brief 创建串口通信实例（真实串口）
    /// @param port 串口设备路径
    /// @param baudrate 波特率
    /// @return 成功返回 SerialComm 指针，失败返回错误信息
    [[nodiscard]] static Expected<std::unique_ptr<SerialComm>> create_real(
        const std::string& port = "/dev/ttyCH341USB0",
        int baudrate = 115200) noexcept;

    /// @brief 创建串口通信实例（自动选择模式）
    /// @param mock 是否使用模拟模式
    /// @param port 串口设备路径
    /// @param baudrate 波特率
    /// @return 成功返回 SerialComm 指针，失败返回错误信息
    /// @note 真实串口打开失败时自动回退模拟模式
    [[nodiscard]] static Expected<std::unique_ptr<SerialComm>> create(
        bool mock = true,
        const std::string& port = "/dev/ttyCH341USB0",
        int baudrate = 115200) noexcept;

    SerialComm(SerialComm&&) noexcept = default;
    SerialComm& operator=(SerialComm&&) noexcept = default;
    SerialComm(const SerialComm&) = delete;
    SerialComm& operator=(const SerialComm&) = delete;

    ~SerialComm();

    explicit SerialComm(SerialState state) noexcept : state_(std::move(state)) {}

    /// @brief 启动串口通信（含接收线程）。
    void start();

    // ==== 发送接口 ====

    /// @brief 发送路径规划移动帧（mode=Path, grab=0）
    /// @param angle 移动角度 0/90/180/270（真实值）
    /// @param steps 步数（带符号）
    void send_move_frame(uint16_t angle, int16_t steps);

    /// @brief 发送视觉定位帧（mode=Locate）
    /// @param x 物料 X 坐标 (mm)
    /// @param y 物料 Y 坐标 (mm)
    /// @param grab 抓取指令 0/1
    void send_locate_frame(uint16_t x, uint16_t y, uint8_t grab);

    /// @brief 发送纯抓取帧（mode=Path, steps=0, grab=1）
    void send_grab_frame();

    /// @brief 直接发送原始帧字节（测试用）
    /// @param frame 完整 12 字节帧
    void send_raw_frame(const std::vector<uint8_t>& frame);

    // ==== 回调注册 ====

    /// @brief 注册比赛开始回调（仅触发一次）
    void set_match_start_callback(MatchStartCallback cb) noexcept {
        match_start_cb_ = std::move(cb);
    }
    /// @brief 注册走路完成回调
    void set_move_done_callback(MoveDoneCallback cb) noexcept {
        move_done_cb_ = std::move(cb);
    }
    /// @brief 注册抓取完成回调
    void set_grab_done_callback(GrabDoneCallback cb) noexcept {
        grab_done_cb_ = std::move(cb);
    }

    /// @brief 判断当前是否为模拟模式
    [[nodiscard]] bool is_mock() const noexcept {
        return std::holds_alternative<MockSerial>(state_);
    }

private:
    /// @brief 真实串口接收循环：6 字节帧同步解析
    void process_real();
    /// @brief 模拟串口接收循环：根据发送记录产生 done 信号
    void process_mock();
    /// @brief 根据当前模式启动对应接收线程
    void start_thread();
    /// @brief 发送帧字节到串口或模拟输出
    /// @param frame 完整帧字节数组
    void transmit(const std::vector<uint8_t>& frame);
    /// @brief 解析接收缓冲区中的完整帧并派发事件
    void dispatch_feedback(const FeedbackFrame& fb) noexcept;

    SerialState state_;                     ///< 串口状态（variant 表达互斥）

    std::deque<uint8_t> rx_buf_;            ///< 接收缓冲区
    std::mutex rx_mutex_;                   ///< 接收缓冲区互斥锁

    std::thread thread_;                    ///< 接收线程
    std::atomic<bool> running_{false};      ///< 线程运行标志
    std::mutex tx_mutex_;                   ///< 发送互斥锁

    // 比赛开始锁存标志
    bool match_started_ = false;            ///< 是否已收到 match_start（仅触发一次）

    // 三个事件回调
    MatchStartCallback match_start_cb_;
    MoveDoneCallback move_done_cb_;
    GrabDoneCallback grab_done_cb_;
    std::mutex cb_mutex_;                   ///< 回调互斥锁

    // 模拟模式状态
    std::atomic<bool> mock_match_sent_{false};     ///< 是否已发过 match_start
    std::atomic<int64_t> mock_move_done_at_{0};    ///< 预定 move_done 触发的时刻 (ms)
    std::atomic<int64_t> mock_grab_done_at_{0};    ///< 预定 grab_done 触发的时刻 (ms)
    std::atomic<bool> mock_move_pending_{false};   ///< 是否有待触发的 move_done
    std::atomic<bool> mock_grab_pending_{false};   ///< 是否有待触发的 grab_done
};

} // namespace gonxun