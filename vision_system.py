#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
工创赛2025智能物流搬运 - 视觉系统主控

【v3.0 重构说明】
- 原单文件 vision_system.py 拆分为 vision/ 子包，按功能模块化
- 各子模块可独立测试和复用
- 本文件作为顶层主控类 VisionSystem，统一调度所有视觉、串口、相机、滤波模块
- 提供统一图像处理流水线，根据下位机下发模式自动切换识别任务

使用方式:
  python vision_system.py            # 启动完整视觉主循环，实时读取摄像头并处理
  python vision_system.py test       # 运行全模块单元测试用例
  from vision import VisionSystem    # 在其他脚本导入视觉主类二次开发
"""
# 导入OpenCV计算机视觉库：图像读取、绘图、窗口、形态学、轮廓等基础操作
import cv2
# 数值计算库：矩阵、数组、卡尔曼滤波矩阵运算、坐标存储
import numpy as np
# 时间库：计算FPS帧率、循环耗时、延时
import time

# 从vision分包导入全部自定义功能模块、模式常量、串口指令常量
from vision import (
    ColorDetector,          # 色块颜色识别模块（红/绿/蓝物料块定位）
    ThreeRingDetector,      # 三环靶标定位模块（比赛色环校准）
    SixRingDetector,        # 六环评分靶标识别模块（放置精度打分）
    QRDetector,             # 二维码图像识别模块
    TaskCodeParser,         # 二维码任务码解析器，解析搬运任务逻辑
    KalmanFilter,           # 卡尔曼滤波：平滑坐标，消除摄像头抖动噪声
    SerialComm,             # 串口通信封装类：和底层STM32/单片机交互
    CameraManager,          # 双摄像头管理：主视觉相机 + 二维码专用相机
    TaskDisplay,            # 任务信息可视化画布生成器
    ObstacleDetector,       # 障碍物圆形识别模块（避障使用）
    # 工作模式枚举常量（下位机通过串口下发，切换视觉识别逻辑）
    MODE_IDLE,      # 0：待机模式，不执行任何识别
    MODE_COLOR,     # 1：三色物料抓取定位模式
    MODE_RING,      # 3：三环校准靶标定位模式
    MODE_DOCK,      # 4：码垛堆放定位模式
    MODE_QR,        # 9：二维码任务读取模式
    # 串口下发坐标时使用的指令标识
    CMD_COLOR,      # 三色物料坐标指令
    CMD_RING,       # 三环靶标坐标指令
    CMD_DOCK,       # 码垛物料坐标指令
    CMD_QR          # 二维码数据发送指令
)


class VisionSystem:
    """
    视觉系统总控制顶层类
    职责：
        1. 统一实例化所有视觉子模块、串口、相机、滤波器
        2. 封装兼容旧版本单文件代码的属性与接口，平滑升级
        3. 根据串口下发工作模式，分发图像至对应识别函数
        4. 坐标滤波、图像绘制标记、串口数据打包发送
        5. 资源统一管理：相机开关、串口启停、窗口释放
    """

    def __init__(self, serial_mock=True, serial_port='/dev/ttyCH341USB0', baudrate=115200):
        """
        视觉系统构造函数，初始化全部硬件与算法模块
        :param serial_mock: bool，True=模拟串口(无真实单片机，调试用)，False=真实物理串口
        :param serial_port: 串口设备号，Linux下CH341转串口默认 /dev/ttyCH341USB0
        :param baudrate: 串口波特率，和下位机单片机程序保持一致115200
        """
        # 1. 颜色色块识别器，用于红/绿/蓝方形物料块检测
        self.color_detector = ColorDetector()
        # 2. 三环靶标检测器（机器人自动校准）、六环评分检测器（堆放精度打分）
        self.three_ring_detector = ThreeRingDetector()
        self.six_ring_detector = SixRingDetector()
        # 3. 二维码识别器 + 任务码解析器：读取场地二维码获取搬运任务
        self.qr_detector = QRDetector()
        self.task_parser = TaskCodeParser()
        # 4. 圆形障碍物检测器，识别场地圆形障碍用于避障
        self.obstacle_detector = ObstacleDetector()
        # 5. 任务可视化画布生成，生成带任务进度的展示图片
        self.task_display = TaskDisplay()

        # 6. 串口通信实例，封装收发、后台接收线程
        self.serial_comm = SerialComm(
            mock=serial_mock, port=serial_port, baudrate=baudrate
        )

        # 7. 双相机管理器：main_index主视觉相机，qr_index独立扫码相机
        self.camera = CameraManager(main_index=0, qr_index=2)

        # 8. 三组独立卡尔曼滤波器，分别对应左/中/右三个目标坐标平滑
        self.kalman_filters = {
            'kf1': KalmanFilter(),
            'kf2': KalmanFilter(),
            'kf3': KalmanFilter()
        }

        # 当前视觉工作模式，实时从串口模块同步下位机下发指令
        self.unit = MODE_IDLE
        # 预留变量：目标工位编号（任务码解析后赋值）
        self.unit_target = 0

    # ========== 兼容旧单文件代码的属性访问器（旧代码无需大幅修改即可迁移） ==========
    @property
    def color_dist(self):
        """兼容旧代码：直接读取颜色检测器的色块距离缓存"""
        return self.color_detector.color_dist

    @property
    def cap(self):
        """兼容旧代码：返回主相机cv2.VideoCapture实例"""
        return self.camera.cap

    @property
    def cap2(self):
        """兼容旧代码：返回二维码专用相机cv2.VideoCapture实例"""
        return self.camera.cap2

    @property
    def ser(self):
        """兼容旧代码：返回串口底层serial对象"""
        return self.serial_comm.ser

    @property
    def serial_mock(self):
        """兼容旧代码：获取当前是否为模拟串口模式"""
        return self.serial_comm.mock

    @property
    def receive(self):
        """兼容旧代码：串口读取数据方法"""
        return self.serial_comm.receive

    @property
    def send(self):
        """兼容旧代码：串口原始发送字节方法"""
        return self.serial_comm.send

    # ========== 兼容旧单文件代码的接口函数，函数名、入参完全对齐旧版本 ==========
    def color_blocks_position_WL(self, img, color, size_code):
        """兼容旧接口：单种颜色物料块检测定位"""
        return self.color_detector.detect(img, color, size_code)

    def color_circle_position(self, img):
        """兼容旧接口：三环色环靶标检测"""
        return self.three_ring_detector.detect(img)

    def detect_qr_code(self, img):
        """兼容旧接口：二维码识别"""
        return self.qr_detector.detect(img)

    def parse_task_code(self, qr_data):
        """兼容旧接口：解析二维码任务字符串"""
        return self.task_parser.parse(qr_data)

    def generate_task_display(self, task_code, completed_steps=None, width=400, height=200):
        """兼容旧接口：生成任务进度可视化图片"""
        return self.task_display.render(task_code, completed_steps)

    def detect_obstacles(self, img, min_radius=15, max_radius=35):
        """兼容旧接口：场地圆形障碍物检测"""
        return self.obstacle_detector.detect(img)

    def detect_six_rings(self, img):
        """兼容旧接口：六环评分靶标识别"""
        return self.six_ring_detector.detect(img)

    def calc_placement_score(self, ring_id, material_fallen=False):
        """兼容旧接口：根据环位计算物料放置得分"""
        from vision import calc_placement_score
        return calc_placement_score(ring_id, material_fallen)

    def start_serial_thread(self):
        """兼容旧接口：启动串口后台接收线程"""
        self.serial_comm.start()

    def open_cameras(self):
        """兼容旧接口：打开两个摄像头设备"""
        self.camera.open()

    def close_cameras(self):
        """兼容旧接口：关闭摄像头、关闭串口，释放全部硬件资源"""
        self.camera.close()
        self.serial_comm.close()

    def send_coordinates(self, cmd, coords, y_diff=None):
        """兼容旧接口：打包坐标并通过串口发送给单片机"""
        self.serial_comm.send_coordinates(cmd, coords, y_diff)

    def uart_process(self):
        """兼容旧空接口：串口接收逻辑已封装在SerialComm子模块，此处仅保留占位兼容"""
        pass

    def uart_process_mock(self):
        """兼容旧空接口：模拟串口逻辑内置在SerialComm，占位兼容"""
        pass

    # ========== 核心图像主流水线：根据模式分发处理逻辑 ==========
    def process_frame(self, img, unit=None):
        """
        单帧图像统一处理入口
        :param img: 原始摄像头BGR图像数组
        :param unit: 指定工作模式；不传则自动读取串口实时下发模式
        :return: 绘制完标记、文字的处理后图像（用于窗口显示）
        """
        # 未传入模式时，同步串口模块最新工作模式
        if unit is None:
            unit = self.serial_comm.unit

        # 拷贝原图，避免修改原始图像数据
        result_img = img.copy() if img is not None else None
        # 空图像直接返回，防止索引报错
        if result_img is None:
            return None

        # 根据下位机下发模式，进入对应识别分支
        if unit == MODE_COLOR:
            self._process_color(result_img)
        elif unit == MODE_RING:
            self._process_ring(result_img)
        elif unit == MODE_DOCK:
            self._process_dock(result_img)
        elif unit == MODE_QR:
            self._process_qr(result_img)

        # 在画面左下角绘制当前运行模式文字
        mode_text = {0: "IDLE", 1: "COLOR", 3: "CIRCLE", 4: "DOCK", 9: "QR"}.get(unit, f"UNK:{unit}")
        cv2.putText(result_img, f"Mode: {mode_text}", (10, result_img.shape[0] - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
        return result_img

    def _process_color(self, result_img):
        """
        模式1 MODE_COLOR：三色物料抓取定位
        逻辑：识别红、绿、蓝三块物料，计算三者几何中心，卡尔曼平滑后下发坐标
        """
        try:
            # 分别检测三种颜色物料块，最小面积阈值2000滤除噪点色块
            red_pos = self.color_detector.detect(result_img, 'red', 2000)
            green_pos = self.color_detector.detect(result_img, 'green', 2000)
            blue_pos = self.color_detector.detect(result_img, 'blue', 2000)
            # 三种色块全部识别成功才计算中心点
            if all([red_pos, green_pos, blue_pos]):
                # 求取三色块平均中心点
                center_x = int((red_pos[0] + green_pos[0] + blue_pos[0]) / 3)
                center_y = int((red_pos[1] + green_pos[1] + blue_pos[1]) / 3)
                # 组装卡尔曼观测值 [x; y] 二维列向量
                z = np.array([[center_x], [center_y]], dtype=np.float32)
                # 卡尔曼滤波平滑，抑制画面抖动、识别跳变
                filtered = self.kalman_filters['kf1'].filter(z)
                # 浮点滤波坐标转为图像像素整数坐标
                qx0, qy0 = int(filtered[0][0]), int(filtered[1][0])

                # 在图像绘制青色实心圆点标记中心点
                cv2.circle(result_img, (qx0, qy0), 10, (255, 255, 0), -1)
                # 绘制坐标文本
                cv2.putText(result_img, f"Center:({qx0},{qy0})",
                            (qx0 - 60, qy0 + 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 1)
                # 将平滑后的中心点坐标通过串口下发给单片机
                self.serial_comm.send_coordinates(CMD_COLOR, [(qx0, qy0)])
        # 捕获识别异常（色块丢失、轮廓计算错误等），不中断主循环
        except Exception as e:
            print(f"unit=1处理异常: {e}")

    def _process_ring(self, result_img):
        """
        模式3 MODE_RING：三环校准靶标定位
        逻辑：识别左/中/右三个色环，分别滤波，标记并下发三组坐标
        """
        try:
            # 检测三环，返回 (x1,y1,x2,y2,x3,y3) 左、中、右环圆心
            circle_pos = self.three_ring_detector.detect(result_img)
            if circle_pos:
                x1, y1, x2, y2, x3, y3 = circle_pos
                # 三组坐标分别送入独立卡尔曼滤波器平滑
                f1 = self.kalman_filters['kf1'].filter(np.array([[x1], [y1]], dtype=np.float32))
                f2 = self.kalman_filters['kf2'].filter(np.array([[x2], [y2]], dtype=np.float32))
                f3 = self.kalman_filters['kf3'].filter(np.array([[x3], [y3]], dtype=np.float32))
                # 转像素整数
                qx1, qy1 = int(f1[0][0]), int(f1[1][0])
                qx2, qy2 = int(f2[0][0]), int(f2[1][0])
                qx3, qy3 = int(f3[0][0]), int(f3[1][0])

                # 循环绘制三个环标记：L左 M中 R右
                for pos, label in [((qx1, qy1), "L"), ((qx2, qy2), "M"), ((qx3, qy3), "R")]:
                    cv2.circle(result_img, pos, 8, (0, 255, 255), -1)
                    cv2.putText(result_img, label, (pos[0] - 5, pos[1] - 15),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
                # 串口下发三组平滑后环圆心坐标
                self.serial_comm.send_coordinates(
                    CMD_RING, [(qx1, qy1), (qx2, qy2), (qx3, qy3)]
                )
        except Exception as e:
            print(f"unit=3处理异常: {e}")

    def _process_dock(self, result_img):
        """
        模式4 MODE_DOCK：码垛堆放定位
        逻辑：识别蓝/绿/红三色堆叠物料，分别滤波标记下发，用于精准码垛
        """
        try:
            # 面积阈值提升至3000，适配远距离码垛物料
            blue_pos = self.color_detector.detect(result_img, 'blue', 3000)
            green_pos = self.color_detector.detect(result_img, 'green', 3000)
            red_pos = self.color_detector.detect(result_img, 'red', 3000)
            # 三色物料全部识别成功再处理
            if all([blue_pos, green_pos, red_pos]):
                # 三组坐标独立卡尔曼平滑
                f1 = self.kalman_filters['kf1'].filter(np.array([[blue_pos[0]], [blue_pos[1]]], dtype=np.float32))
                f2 = self.kalman_filters['kf2'].filter(np.array([[green_pos[0]], [green_pos[1]]], dtype=np.float32))
                f3 = self.kalman_filters['kf3'].filter(np.array([[red_pos[0]], [red_pos[1]]], dtype=np.float32))
                qx1, qy1 = int(f1[0][0]), int(f1[1][0])
                qx2, qy2 = int(f2[0][0]), int(f2[1][0])
                qx3, qy3 = int(f3[0][0]), int(f3[1][0])

                # 分别用对应颜色绘制标记：B蓝 G绿 R红
                for pos, color, label in [
                    ((qx1, qy1), (255, 0, 0), "B"),
                    ((qx2, qy2), (0, 255, 0), "G"),
                    ((qx3, qy3), (0, 0, 255), "R")
                ]:
                    cv2.circle(result_img, pos, 8, color, -1)
                    cv2.putText(result_img, label, (pos[0] - 5, pos[1] - 15),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1)
                # 下发码垛三色物料坐标
                self.serial_comm.send_coordinates(
                    CMD_DOCK, [(qx1, qy1), (qx2, qy2), (qx3, qy3)]
                )
        except Exception as e:
            print(f"unit=4处理异常: {e}")

    def _process_qr(self, result_img):
        """
        模式9 MODE_QR：二维码任务读取
        逻辑：切换至独立二维码相机读取画面，识别二维码，解析后通过串口下发任务字符串
        """
        try:
            # 读取二维码专用相机一帧图像
            success, qr_img = self.camera.read_qr()
            # 扫码相机读取失败则降级使用主相机画面识别二维码
            target_img = qr_img if success else result_img
            if target_img is None:
                return
            # 调用二维码识别模块解析内容
            qr_data = self.qr_detector.detect(target_img)
            # 识别到有效二维码数据
            if qr_data:
                print(f"二维码识别成功: {qr_data}")
                # 在主画面左上角打印二维码文本
                cv2.putText(result_img, f"QR: {qr_data}", (10, 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                # 串口下发二维码原始字符串给下位机解析任务
                self.serial_comm.send_qr_data(qr_data)
        except Exception as e:
            print(f"unit=9处理异常: {e}")


def main():
    """
    程序入口主函数
    完整运行流程：打印启动信息 → 实例化视觉系统 → 启动串口线程 → 打开双相机
    循环读取主相机帧 → 处理图像 → 窗口显示 + FPS统计 → q键退出
    兼容无GUI服务器环境（headless无图形窗口，仅控制台打印日志）
    """
    print("=" * 50)
    print("工创赛2025智能物流视觉系统 v3.0 (模块化)")
    print("=" * 50)

    # 实例化视觉系统，默认开启模拟串口（调试不接单片机）
    vision = VisionSystem(serial_mock=True)
    # 启动串口后台接收线程，实时监听下位机下发模式指令
    vision.serial_comm.start()
    # 打开主视觉相机、二维码相机
    vision.camera.open()

    print("\n系统已启动，按 'q' 退出")
    print("模式说明: 0=待机, 1=三色物料, 3=色环定位, 4=码垛定位, 9=二维码")

    # 检测当前运行环境是否支持OpenCV图形窗口GUI
    gui_available = True
    try:
        # 尝试创建销毁临时窗口，报错则说明无图形界面（Linux服务器、WSL无桌面）
        cv2.namedWindow('test')
        cv2.destroyWindow('test')
    except Exception:
        gui_available = False
        print("[注意] Windows headless模式，不支持cv2.imshow图形窗口")

    # FPS帧率统计变量
    fps = 0.0
    fps_start_time = time.time()
    fps_frame_count = 0
    FPS_UPDATE_INTERVAL = 10  # 每累积10帧刷新一次FPS数值，避免频繁计算

    try:
        # 主图像循环，持续读取相机处理
        while True:
            loop_start = time.time()
            # 读取主视觉相机单帧图像
            success, img = vision.camera.read_main()
            if success:
                # 送入流水线处理图像，绘制标记文字
                processed_img = vision.process_frame(img)
                # 有图形界面：弹出窗口实时显示画面
                if gui_available:
                    fps_frame_count += 1
                    # 达到更新帧数阈值，重新计算平均FPS
                    if fps_frame_count >= FPS_UPDATE_INTERVAL:
                        elapsed = time.time() - fps_start_time
                        fps = fps_frame_count / elapsed
                        fps_frame_count = 0
                        fps_start_time = time.time()
                    # 在画面右上角绘制实时FPS
                    cv2.putText(processed_img, f"FPS: {fps:.1f}", (processed_img.shape[1] - 120, 30),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
                    # 弹出图像窗口
                    cv2.imshow('Vision System', processed_img)
                    # 等待1ms按键，按下q退出循环
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break
                # 无GUI环境：控制台打印模式与帧率，0.5s打印一次减少刷屏
                else:
                    fps_frame_count += 1
                    if fps_frame_count >= 2:
                        elapsed = time.time() - fps_start_time
                        fps = fps_frame_count / elapsed if elapsed > 0 else 0
                        fps_frame_count = 0
                        fps_start_time = time.time()
                    print("[帧] 模式={} FPS={:.1f}".format(vision.serial_comm.unit, fps))
                    time.sleep(0.5)
            # 相机读取失败（断开、无设备），生成纯黑测试画布替代
            else:
                # 480*640 黑色测试背景
                test_img = np.zeros((480, 640, 3), dtype=np.uint8)
                test_img[:] = (50, 50, 50)
                # 绘制三个彩色模拟物料圆，方便离线调试
                cv2.circle(test_img, (200, 200), 30, (0, 0, 255), -1)
                cv2.circle(test_img, (320, 200), 30, (0, 255, 0), -1)
                cv2.circle(test_img, (440, 200), 30, (255, 0, 0), -1)
                processed_img = vision.process_frame(test_img)
                # 有GUI则弹出测试画面窗口
                if gui_available:
                    fps_frame_count += 1
                    if fps_frame_count >= FPS_UPDATE_INTERVAL:
                        elapsed = time.time() - fps_start_time
                        fps = fps_frame_count / elapsed
                        fps_frame_count = 0
                        fps_start_time = time.time()
                    cv2.putText(processed_img, f"FPS: {fps:.1f}", (processed_img.shape[1] - 120, 30),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
                    cv2.imshow('Vision System (Test)', processed_img)
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break
                # 无GUI控制台打印测试帧日志
                else:
                    print("[测试帧] 模式={}".format(vision.serial_comm.unit))
                    time.sleep(0.5)
    # 捕获Ctrl+C手动终止程序
    except KeyboardInterrupt:
        print("\n用户中断")
    # 无论正常退出、异常中断，最终统一释放资源
    finally:
        vision.close_cameras()
        # 关闭所有OpenCV图像窗口
        if gui_available:
            try:
                cv2.destroyAllWindows()
            except Exception:
                pass
        print("系统已关闭")


def run_unit_tests():
    """
    单元测试入口函数
    导入测试套件并执行全部模块功能测试，用于验证各识别算法逻辑正确性
    """
    import sys
    from tests.test_vision import run_all_tests
    return run_all_tests()


# 程序执行入口判断：直接运行该脚本才执行main，import导入时不自动启动主循环
if __name__ == "__main__":
    import sys
    # 命令行参数携带test，则执行单元测试
    if len(sys.argv) > 1 and sys.argv[1] == 'test':
        success = run_unit_tests()
        sys.exit(0 if success else 1)
    # 无参数正常启动视觉主程序
    else:
        main()