"""
视觉系统核心调度模块
VisionSystem 统一管理视觉、串口、相机、滤波模块，
根据下位机下发模式自动切换识别任务。
"""
# 导入视觉、数值计算标准库
import cv2
import numpy as np
# 日志模块，统一打印调试/报错信息，替代零散print
import logging
# 全局配置文件，所有阈值、硬件参数统一存放
import config

# 创建当前文件专属日志对象，日志会标注来源模块，方便定位问题
logger = logging.getLogger(__name__)

# 相对导入：同目录下自研视觉算法类（包内文件才能使用.相对导入）
# 色块识别器：HSV阈值识别红/绿/蓝物料
from .color_detector import ColorDetector
# 环形靶标识别：三环、六环同心圆检测
from .ring_detector import ThreeRingDetector, SixRingDetector
# 二维码识别器 + 二维码任务字符串解析工具
from .qr_detector import QRDetector, TaskCodeParser
# 卡尔曼滤波器：平滑目标坐标，消除识别抖动、补全短时遮挡
from .kalman_filter import KalmanFilter
# 串口通信模块：上位机 ↔ 下位单片机通信，导入通信类、工作模式、下发指令常量
from .serial_comm import (
    SerialComm, MODE_IDLE, MODE_COLOR, MODE_RING, MODE_DOCK, MODE_QR,
    CMD_COLOR, CMD_RING, CMD_DOCK, CMD_QR,
)
# 相机管理类：统一管理主视觉相机、扫码相机，封装打开/读帧/重连逻辑
from .camera_manager import CameraManager
# 画面可视化工具：在原图绘制识别框、中心点、文字状态
from .task_display import TaskDisplay
# 障碍物轮廓检测，用于机器人避障
from .obstacle_detector import ObstacleDetector


class VisionSystem:
    """视觉系统总控制类，统一调度所有子模块
    顶层入口类，整合相机、识别算法、滤波、串口通信；
    根据下位机下发的工作模式，自动执行对应视觉识别流水线
    """

    # 模式数字 → 文本映射字典，画面打印状态文字使用
    _MODE_TEXT = {0: "IDLE", 1: "COLOR", 3: "RING", 4: "DOCK", 9: "QR"}

    def __init__(self, serial_mock=None, serial_port=None, baudrate=None,
                 main_camera=None, qr_camera=None):
        """
        初始化全部硬件与算法模块
        构造函数：创建所有识别器、相机、串口、卡尔曼滤波实例

        :param serial_mock: True=模拟串口(离线调试不接硬件) False=真实硬件串口；不传/None读取config全局默认
        :param serial_port: 串口设备号 Windows:"COM3" Linux:"/dev/ttyUSB0"；None读配置
        :param baudrate: 串口通信波特率，如115200；None读配置
        :param main_camera: 主视觉相机参数，支持两种格式：单数字索引 / (索引,宽,高)元组；None读配置
        :param qr_camera: 二维码扫码相机参数，格式同上；None读配置
        """
        # 三行参数优先级逻辑：外部传入参数 > config全局默认值
        # 外部传有效值则使用传入值，不传(None)则读取配置文件预设
        serial_mock = config.SERIAL_MOCK if serial_mock is None else serial_mock
        serial_port = config.SERIAL_PORT if serial_port is None else serial_port
        baudrate = config.SERIAL_BAUDRATE if baudrate is None else baudrate

        # 实例化各类视觉识别算法对象，全局复用，避免重复创建开销
        self.color_detector = ColorDetector()               # 色块识别实例
        self.three_ring_detector = ThreeRingDetector()      # 三环靶标识别实例
        self.six_ring_detector = SixRingDetector()          # 六环靶标识别实例（当前代码未使用，预留扩展）
        self.qr_detector = QRDetector()                    # 二维码识别实例
        self.task_parser = TaskCodeParser()                 # 二维码任务解析器（预留扩展）
        self.obstacle_detector = ObstacleDetector()        # 障碍物检测实例（预留扩展）
        self.task_display = TaskDisplay()                  # 可视化绘制工具实例

        # 初始化串口通信对象，传入处理后的串口参数与模拟周期配置
        self.serial_comm = SerialComm(
            mock=serial_mock, port=serial_port, baudrate=baudrate,
            mock_cycle=config.SERIAL_MOCK_CYCLE
        )

        # 初始化相机管理类
        # _cam_idx静态方法：解析相机入参，只提取设备索引，分辨率读取全局config
        self.camera = CameraManager(
            main_index=self._cam_idx(main_camera, config.CAMERA_MAIN_INDEX),
            qr_index=self._cam_idx(qr_camera, config.CAMERA_QR_INDEX),
            main_width=config.CAMERA_MAIN_WIDTH, main_height=config.CAMERA_MAIN_HEIGHT,
            qr_width=config.CAMERA_QR_WIDTH, qr_height=config.CAMERA_QR_HEIGHT
        )

        # 创建3个卡尔曼滤波实例，分别对应3路目标坐标平滑（三色/三环三个点位）
        # Q过程噪声、R观测噪声从配置读取，控制平滑强度
        self.kalman_filters = [
            KalmanFilter(q=config.KALMAN_Q, r=config.KALMAN_R) for _ in range(3)
        ]

    @staticmethod
    def _cam_idx(camera_arg, default_idx):
        """解析摄像头索引：支持 (index, w, h) 元组或单索引
        静态工具方法，无self，仅内部调用
        兼容两种相机传参格式，统一提取设备编号

        :param camera_arg: 用户传入的相机参数，数字/元组/None
        :param default_idx: 配置文件默认相机索引，入参为None时使用
        :return: 最终确定的相机设备数字索引
        """
        # 如果传入元组/列表，只取第一个元素作为相机编号，宽高忽略（宽高统一走config）
        if isinstance(camera_arg, (tuple, list)):
            return camera_arg[0]
        # 入参为None返回默认索引；否则直接返回传入数字索引
        return default_idx if camera_arg is None else camera_arg

    # ========== 核心图像处理流水线 ==========
    def process_frame(self, img: np.ndarray, unit: int = None) -> np.ndarray:
        """
        单帧图像统一处理入口
        每一帧画面的总调度函数：根据当前工作模式分发对应识别逻辑，绘制状态文字并返回标注完成的图像

        :param img: 原始BGR格式图像（OpenCV默认色彩空间）
        :param unit: 工作模式编号；不传/None则自动从串口读取下位机下发的实时模式
        :return: 绘制识别标记、模式文字后的处理图像；原图为空则返回None
        """
        # 图像为空直接返回，防止空数组报错
        if img is None:
            return None

        # 未手动指定模式，则从串口通信实例读取下位机下发的当前工作模式
        if unit is None:
            unit = self.serial_comm.unit

        # 拷贝原图，所有绘制操作在副本上执行，不破坏原始图像数据
        result_img = img.copy()
        # 模式-处理函数字典：映射每个工作模式对应的私有处理函数，简化if-elif分支
        handlers = {
            MODE_COLOR: self._process_color,
            MODE_RING: self._process_ring,
            MODE_DOCK: self._process_dock,
            MODE_QR: self._process_qr,
        }
        # 根据模式取出对应处理函数，无匹配模式则handler为None，跳过识别
        handler = handlers.get(unit)
        if handler:
            handler(result_img)

        # 将数字模式转为可读文本，未知模式打印UNK+编号
        mode_text = self._MODE_TEXT.get(unit, f"UNK:{unit}")
        # 在画面左下角绘制当前运行模式文字，黄色字体
        cv2.putText(result_img, f"Mode: {mode_text}", (10, result_img.shape[0] - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
        # 返回标注完成的图像，用于窗口展示/保存
        return result_img

    def _filter_position(self, x, y, kf_index):
        """对坐标进行卡尔曼滤波，返回整数像素坐标
        单路坐标平滑工具函数，输入原始识别坐标，输出滤波稳定后的整数坐标

        :param x: 原始识别x像素坐标
        :param y: 原始识别y像素坐标
        :param kf_index: 使用第几个卡尔曼滤波器（0/1/2，对应3个目标）
        :return: (滤波后x, 滤波后y) 整数像素坐标
        """
        # 封装为2行1列numpy矩阵，符合卡尔曼输入格式
        filtered = self.kalman_filters[kf_index].filter(
            np.array([[x], [y]], dtype=np.float32)
        )
        # 浮点数坐标转整数像素，返回元组
        return int(filtered[0][0]), int(filtered[1][0])

    def _detect_three_colors(self, img, color_specs, min_area, max_area):
        """
        检测三种颜色并滤波、绘制
        通用三色识别工具函数：颜色识别、坐标滤波、画面绘制三合一
        颜色任务、码垛停靠任务共用此函数，仅传入的颜色配置不同

        :param img: 需要绘制标注的图像副本
        :param color_specs: 三色配置列表，格式[(颜色名, 画面标签, 绘制RGB颜色), ...]
        :param max_area: 色块最大面积阈值，大于该面积的色块判定为异常过滤
        :param min_area: 色块最小面积阈值，小于该面积的色块判定为噪声过滤
        :return: 滤波后三色坐标列表；任意一种颜色未检测到则返回None，不向下位机发数据
        """
        positions = []
        # 循环遍历三种目标颜色，依次执行色块识别
        for color, _, _ in color_specs:
            pos = self.color_detector.detect(img, color, min_area, max_area)
            # 任意一个颜色缺失，直接返回None，中断本次任务
            if pos is None:
                return None
            positions.append(pos)

        # 对三个色块原始坐标分别使用对应编号卡尔曼滤波平滑
        filtered = [self._filter_position(pos[0], pos[1], idx)
                    for idx, pos in enumerate(positions)]
        # 遍历滤波坐标，在图像上绘制实心圆点+文字标签
        for (x, y), (_, label, draw_color) in zip(filtered, color_specs):
            cv2.circle(img, (x, y), 8, draw_color, -1)
            cv2.putText(img, label, (x - 5, y - 15),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, draw_color, 1)
        # 返回三组平滑后的坐标，用于串口下发
        return filtered

    def _process_color(self, result_img):
        """模式1：三色物料抓取定位
        MODE_COLOR 对应业务：抓取红、绿、蓝三色物料，识别后下发坐标给下位机
        """
        try:
            # 三色识别配置：颜色名称、画面显示标签、绘制圆点颜色(BGR顺序)
            color_specs = [
                ('red', 'R', (0, 0, 255)),
                ('green', 'G', (0, 255, 0)),
                ('blue', 'B', (255, 0, 0))
            ]
            # 调用通用三色检测函数，最小色块面积2000，最大色块面积10000，过滤微小噪声色块
            filtered = self._detect_three_colors(result_img, color_specs, 2000, 10000    )
            # 三色全部识别成功，通过串口下发坐标指令CMD_COLOR
            if filtered:
                self.serial_comm.send_coordinates(CMD_COLOR, filtered)
        # 捕获本模式下所有异常，打印详细错误堆栈，避免单帧识别崩溃导致整个视觉系统卡死
        except Exception as e:
            logger.error(f"unit=1处理异常: {e}", exc_info=True)

    def _process_ring(self, result_img):
        """模式3：三环校准靶标定位
        MODE_RING 对应业务：识别场地三环同心圆靶标，输出左/中/右三个圆心坐标下发下位机校准
        """
        try:
            # 三环识别，返回包含三组(x,y)的一维数组 [x0,y0,x1,y1,x2,y2]
            circle_pos = self.three_ring_detector.detect(result_img)
            # 未识别到三环直接退出，不发送数据
            if circle_pos is None:
                return

            # 拆分三组圆心坐标，分别使用对应卡尔曼滤波平滑
            filtered = [self._filter_position(circle_pos[i * 2], circle_pos[i * 2 + 1], idx)
                        for idx in range(3)]
            # 在图像绘制三环中心点与L/M/R标签（黄点）
            for pos, label in zip(filtered, ("L", "M", "R")):
                cv2.circle(result_img, pos, 8, (0, 255, 255), -1)
                cv2.putText(result_img, label, (pos[0] - 5, pos[1] - 15),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
            # 串口下发三环坐标指令CMD_RING
            self.serial_comm.send_coordinates(CMD_RING, filtered)
        # 异常捕获，打印日志不崩溃主线程
        except Exception as e:
            logger.error(f"unit=3处理异常: {e}", exc_info=True)

    def _process_dock(self, result_img):
        """模式4：码垛堆放定位
        MODE_DOCK 对应业务：物料码垛停靠点位识别，同样识别蓝绿红三色，面积阈值更大3000，过滤细碎干扰
        """
        try:
            # 码垛三色识别配置，顺序调换为蓝、绿、红
            color_specs = [
                ('blue', 'B', (255, 0, 0)),
                ('green', 'G', (0, 255, 0)),
                ('red', 'R', (0, 0, 255)),
            ]
            # 码垛色块体积更大，最小识别面积3000，最大识别面积10000，过滤地面小色块干扰
            filtered = self._detect_three_colors(result_img, color_specs, 3000, 10000)
            # 识别完整则下发码垛指令CMD_DOCK
            if filtered:
                self.serial_comm.send_coordinates(CMD_DOCK, filtered)
        except Exception as e:
            logger.error(f"unit=4处理异常: {e}", exc_info=True)

    def _process_qr(self, result_img):
        """模式9：二维码任务读取
        MODE_QR 对应业务：切换扫码相机读取二维码，解析任务字符串并下发给下位机
        """
        try:
            # 调用相机管理器读取二维码相机画面，success标记相机是否正常打开
            success, qr_img = self.camera.read_qr()
            # 扫码相机正常则使用扫码画面识别；相机异常降级使用主相机画面识别
            target_img = qr_img if success else result_img
            # 图像为空直接跳过
            if target_img is None:
                return

            # 执行二维码解码，返回二维码文本内容
            qr_data = self.qr_detector.detect(target_img)
            # 成功识别二维码
            if qr_data:
                logger.info(f"二维码识别成功: {qr_data}")
                # 在主画面左上角打印二维码字符串，绿色文字
                cv2.putText(result_img, f"QR: {qr_data}", (10, 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                # 串口下发二维码文本指令CMD_QR
                self.serial_comm.send_qr_data(qr_data)
        # 捕获扫码识别、相机读取所有异常
        except Exception as e:
            logger.error(f"unit=9处理异常: {e}", exc_info=True)