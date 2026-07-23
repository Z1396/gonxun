/**
 * @file serial_comm.hpp
 * @brief 串口通信模块，支持真实硬件串口和模拟模式
 *
 * 负责上位机（Jetson）与下位机（STM32）之间的串口通信。
 * 接收端持续读取下位机的工作模式字节，发送端按协议帧格式
 * 将坐标/QR数据打包后写入串口。模拟模式下自动周期切换工作模式。
 *
 * 帧格式 (15字节):
 *   [0]      帧头 0x66
 *   [1]      命令字节
 *   [2..13]  数据域 (12字节)
 *   [14]     校验和 (cmd + data 累加和 & 0xFF)
 *   [15]     帧尾 0x77
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// ==== 帧格式常量 ====
constexpr uint8_t FRAME_HEADER = 0x66;  ///< 帧头标识字节
constexpr uint8_t FRAME_TAIL = 0x77;    ///< 帧尾标识字节

// ==== 帧结构索引 ====
constexpr std::size_t FRAME_DATA_LEN = 12;      ///< 数据域长度 (字节)
constexpr std::size_t FRAME_CHECKSUM_IDX = 13;  ///< 校验和在帧中的下标
constexpr std::size_t FRAME_TAIL_IDX = 14;      ///< 帧尾在帧中的下标

// ==== 工作模式（下位机下发） ====
constexpr uint8_t MODE_IDLE = 0;  ///< 空闲模式
constexpr uint8_t MODE_COLOR = 1; ///< 颜色识别模式
constexpr uint8_t MODE_RING = 3;  ///< 圆环检测模式
constexpr uint8_t MODE_DOCK = 4;  ///< 对接停靠模式
constexpr uint8_t MODE_QR = 9;    ///< 二维码扫描模式

// ==== 命令字节（上位机下发） ====
constexpr uint8_t CMD_COLOR = 0x01; ///< 颜色坐标命令
constexpr uint8_t CMD_RING = 0x03;  ///< 圆环坐标命令
constexpr uint8_t CMD_DOCK = 0x04;  ///< 对接坐标命令
constexpr uint8_t CMD_QR = 0x09;    ///< QR数据命令

/**
 * @brief 串口通信类，支持真实串口和模拟模式
 *
 * 接收线程持续读取串口数据并解析工作模式；发送函数按帧协议
 * 打包数据后写入串口。模拟模式下接收线程自动周期切换工作模式，
 * 发送函数仅打印帧内容到标准输出。
 */
class SerialComm {
public:
    /**
     * @brief 构造串口通信对象
     * @param mock 是否使用模拟模式（默认 true）
     * @param port 串口设备路径，如 "/dev/ttyCH341USB0"
     * @param baudrate 波特率，默认 115200
     * @param mock_unit 模拟模式下固定的工作模式字节
     * @param mock_cycle 模拟模式下是否周期切换工作模式
     */
    explicit SerialComm(bool mock = true,
                        const std::string& port = "/dev/ttyCH341USB0",
                        int baudrate = 115200,
                        uint8_t mock_unit = MODE_IDLE,
                        bool mock_cycle = false);
    ~SerialComm();

    SerialComm(const SerialComm&) = delete;
    SerialComm& operator=(const SerialComm&) = delete;

    /**
     * @brief 打开真实串口设备
     * @return 打开成功返回 true，失败返回 false
     * @note 仅打开串口，不启动接收线程
     */
    [[nodiscard]] bool open();

    /** @brief 关闭串口并停止接收线程 */
    void close();

    /**
     * @brief 启动串口通信（含接收线程）
     * @note 若真实串口打开失败，自动回退到模拟模式
     */
    void start();

    /**
     * @brief 发送坐标数据帧
     * @param cmd 命令字节 (CMD_COLOR / CMD_RING / CMD_DOCK)
     * @param coords 坐标列表，每对为 (x, y)
     * @note CMD_COLOR 编码3组(x,y)；CMD_RING/CMD_DOCK 编码中间圆心(x2,y2)和y差值
     */
    void send_coordinates(uint8_t cmd, const std::vector<std::pair<int, int>>& coords);

    /**
     * @brief 发送二维码数据帧
     * @param qr_data QR码字符串，截断至 FRAME_DATA_LEN 字节
     */
    void send_qr_data(const std::string& qr_data);

    /**
     * @brief 直接发送原始帧数据
     * @param frame 完整帧字节数组
     */
    void send_raw_frame(const std::vector<uint8_t>& frame);

    std::atomic<uint8_t> unit{MODE_IDLE};       ///< 当前工作模式（原子读取）
    std::atomic<uint8_t> unit_target{0};         ///< 当前目标编号（原子读取）

private:
    /** @brief 真实串口接收循环，解析4字节帧头+模式+目标 */
    void process_real();
    /** @brief 模拟串口接收循环，周期或固定设置 unit */
    void process_mock();
    /** @brief 根据模式启动对应接收线程 */
    void start_thread();
    /**
     * @brief 构建发送帧
     * @param cmd 命令字节
     * @param data_bytes 数据域字节
     * @note 帧格式: [HEADER][CMD][DATA×12][CHECKSUM][TAIL]
     */
    void build_frame(uint8_t cmd, const std::vector<uint8_t>& data_bytes);
    /**
     * @brief 发送帧数据到串口或模拟输出
     * @param frame 完整帧字节数组
     */
    void transmit(const std::vector<uint8_t>& frame);

    bool mock_;                 ///< 是否模拟模式
    std::string port_;          ///< 串口设备路径
    int baudrate_;              ///< 波特率
    uint8_t mock_unit_;         ///< 模拟固定模式字节
    bool mock_cycle_;           ///< 模拟周期切换开关

    int fd_;                    ///< 串口文件描述符，-1 表示未打开
    std::vector<uint8_t> receive_; ///< 接收缓冲区 (4字节)
    std::vector<uint8_t> send_;    ///< 发送帧缓冲区 (15字节)

    std::thread thread_;        ///< 接收线程
    std::atomic<bool> running_; ///< 线程运行标志
    std::mutex mutex_;          ///< 收发缓冲区互斥锁

    std::vector<uint8_t> mock_cycle_units_; ///< 模拟周期模式序列
    std::size_t mock_cycle_idx_;            ///< 模拟周期当前下标
};
