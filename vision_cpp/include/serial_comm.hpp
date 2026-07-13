// 串口通信模块头文件
// - 真实串口接收 (与下位机STM32通信)
// - 模拟串口 (无硬件时使用)
// 协议：帧头0x66 + 命令 + 数据 + 校验和 + 帧尾0x77
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// 串口协议常量（字节值）
constexpr uint8_t FRAME_HEADER = 0x66;  // 帧头
constexpr uint8_t FRAME_TAIL = 0x77;    // 帧尾

// 帧结构索引
constexpr std::size_t FRAME_DATA_LEN = 12;      // 数据段长度
constexpr std::size_t FRAME_CHECKSUM_IDX = 13;  // 校验和位置
constexpr std::size_t FRAME_TAIL_IDX = 14;      // 帧尾位置

// 工作模式（下位机下发）
constexpr uint8_t MODE_IDLE = 0;
constexpr uint8_t MODE_COLOR = 1;
constexpr uint8_t MODE_RING = 3;
constexpr uint8_t MODE_DOCK = 4;
constexpr uint8_t MODE_QR = 9;

// 命令字节（上位机下发）
constexpr uint8_t CMD_COLOR = 0x01;
constexpr uint8_t CMD_RING = 0x03;
constexpr uint8_t CMD_DOCK = 0x04;
constexpr uint8_t CMD_QR = 0x09;

// 串口通信类，支持真实硬件串口和无硬件模拟模式
class SerialComm {
public:
    SerialComm(bool mock = true,
               const std::string& port = "/dev/ttyCH341USB0",
               int baudrate = 115200,
               uint8_t mock_unit = MODE_IDLE,
               bool mock_cycle = false);
    ~SerialComm();

    // 禁止拷贝（持有线程与文件描述符）
    SerialComm(const SerialComm&) = delete;
    SerialComm& operator=(const SerialComm&) = delete;

    // 打开串口（真实模式），成功返回 true
    bool open();
    // 关闭串口并停止接收线程
    void close();
    // 启动串口接收线程；真实模式打开失败时回退到模拟模式
    void start();

    // 发送坐标到下位机
    void sendCoordinates(uint8_t cmd, const std::vector<std::pair<int, int>>& coords);
    // 发送二维码数据
    void sendQrData(const std::string& qr_data);

    // 公开成员变量（下位机下发的模式），原子变量保证多线程读取安全
    std::atomic<uint8_t> unit{MODE_IDLE};
    std::atomic<uint8_t> unit_target{0};

private:
    void processReal();  // 真实串口接收循环
    void processMock();  // 模拟串口接收循环
    void startThread();  // 启动后台线程
    // 构建发送帧（调用者需持有 mutex_）
    void buildFrame(uint8_t cmd, const std::vector<uint8_t>& data_bytes);
    // 真实/模拟发送
    void transmit(const std::vector<uint8_t>& frame);

    bool mock_;
    std::string port_;
    int baudrate_;
    uint8_t mock_unit_;
    bool mock_cycle_;

    int fd_;  // 串口文件描述符，-1 表示未打开
    std::vector<uint8_t> receive_;
    std::vector<uint8_t> send_;

    std::thread thread_;
    std::atomic<bool> running_;
    std::mutex mutex_;  // 保护 receive_ / send_

    std::vector<uint8_t> mock_cycle_units_;
    std::size_t mock_cycle_idx_;
};
