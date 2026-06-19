#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
工创赛2025智能物流搬运机器人 Web可视化仿真程序 V5.0
项目功能：
1. 还原2400mm标准比赛场地，绘制车道、加工区、原料转盘、启停区、二维码点位、随机障碍物
2. 自动生成随机四段式比赛任务码，机器人全自动执行完整业务流程
   流程：前往二维码板扫码 → 原料区抓取3个物料 → 粗加工区放置 → 暂存区放置 → 返回起点
3. 双视图渲染：全局场地俯视图 + 车载相机仿真画面
4. 实时信息面板：任务码、批次物料、进度条、总分、当前动作、任务列表、模拟串口日志、小车坐标帧率
5. Flask搭建HTTP网页服务，MJPG实时视频流，局域网所有设备可同步访问画面
6. 配套接口：页面首页、实时视频、一键重置新对局、前端获取全部仿真状态JSON接口
技术依赖：Python3 + OpenCV + Numpy + Pillow(PIL) + Flask
运行安装依赖：pip install opencv-python numpy flask pillow
"""

# OpenCV计算机视觉库：画布创建、几何图形绘制、图像缩放、jpg编码输出
import cv2
# 数值计算库：存储图像像素矩阵、小车浮点坐标向量、欧氏距离、数组运算
import numpy as np
# Python内置数学库：三角函数、角度弧度转换、点位几何计算
import math
# Flask Web框架核心组件：网站实例、页面渲染、流式响应、json返回
from flask import Flask, render_template, Response, jsonify, request
# 内置时间库：统计FPS帧率、生成日志时间戳、画面渲染延时控制
import time
# 内置随机库：打乱物料序列、随机生成车道障碍物坐标、随机环号
import random
# PIL图像库：解决OpenCV原生putText不支持中文乱码问题
from PIL import Image, ImageDraw, ImageFont

# 创建Flask网站实例对象
# __name__内置变量自动定位templates模板文件夹、static静态资源文件夹
app = Flask(__name__)


# 字体缓存字典：按 font_size 缓存已加载的 PIL 字体对象，避免每帧重复加载
_FONT_CACHE = {}


def put_chinese_text(img, text, pos, font_size=16, color=(255, 255, 255)):
    """
    OpenCV画布中文绘制兼容工具函数
    cv2原生文字函数仅支持英文/数字，中文会显示方块，借助PIL绘制后转回BGR格式
    :param img: 输入OpenCV BGR格式画布
    :param text: 需要绘制的中文/混合文本字符串
    :param pos: 文字左上角像素坐标 (x, y)
    :param font_size: 字体字号，默认16
    :param color: 文字RGB颜色元组，默认白色(255,255,255)
    :return: 绘制完成后的OpenCV BGR图像
    """
    # 从缓存获取字体，首次加载时按 size 缓存
    font = _FONT_CACHE.get(font_size)
    if font is None:
        try:
            font = ImageFont.truetype("C:/Windows/Fonts/msyh.ttc", font_size)
        except Exception:
            try:
                font = ImageFont.truetype("msyh.ttc", font_size)
            except Exception:
                font = ImageFont.load_default()
        _FONT_CACHE[font_size] = font

    # OpenCV图像通道为BGR，PIL绘图必须转换为RGB通道
    pil_img = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
    # 创建PIL画笔对象，用于画布文字绘制
    draw = ImageDraw.Draw(pil_img)
    # 在指定坐标写入文字，fill参数接收RGB颜色
    draw.text(pos, text, font=font, fill=color)
    # 绘图完成，将RGB图像转回OpenCV标准BGR格式返回
    return cv2.cvtColor(np.array(pil_img), cv2.COLOR_RGB2BGR)


# ========== 全局场地物理常量（单位：毫米 mm）==========
# 场地正方形边长2400mm
FIELD_SIZE = 2400
# 毫米转像素缩放系数：1毫米对应0.22像素，控制画布整体分辨率大小
PIXEL_PER_MM = 0.22

# 物料颜色映射字典
# key：物料数字编号；value：(OpenCV BGR色彩值, 颜色英文缩写标识)
# OpenCV色彩通道顺序：蓝(B)、绿(G)、红(R)
COLOR_MAP = {
    1: ((0, 0, 255), "RED"),    # 红色物料
    2: ((0, 200, 255), "YEL"),  # 黄色物料
    3: ((255, 0, 0), "BLU"),    # 蓝色物料
    4: ((0, 255, 0), "GRN"),    # 绿色物料
    5: ((30, 30, 30), "BLK"),   # 黑色障碍物
    6: ((200, 220, 255), "LBU"),# 浅蓝色物料
}

# 放置圆环得分规则：key=圆环编号，value=放置该环获得分数
RING_SCORES = {1: 15, 2: 10, 3: 7, 4: 5, 5: 3, 6: 1}

# 十字交叉灰色车道尺寸参数
LANE_WIDTH = 400                   # 车道宽度400mm
LANE_CENTER = FIELD_SIZE // 2      # 场地中心Y坐标 1200mm
LANE_START = LANE_CENTER - LANE_WIDTH // 2  # 车道上下左右边界起始1000mm
LANE_END = LANE_CENTER + LANE_WIDTH // 2    # 车道边界结束1400mm

# 四角淡黄色安全区域 存储格式(x起点, y起点, 宽度w, 高度h)
YELLOW_ZONES = [
    (0, 0, 450, 450),           # 场地左下角
    (1950, 0, 450, 450),        # 场地右下角
    (0, 1950, 450, 450),        # 场地左上角
    (1950, 1950, 450, 450),     # 场地右上角
]

# 机器人蓝色启停起点区域，300*300mm矩形
START_ZONE_1 = (50, 50, 300, 300)       # 左下角起点，程序小车初始位置
START_ZONE_2 = (2050, 50, 300, 300)     # 右下角备用起点

# 原料圆形物料转盘参数
RAW_ZONE_CENTER = (1000, 1200)  # 转盘中心物理坐标 mm
RAW_ZONE_RADIUS = 150           # 转盘半径150mm

# 粗加工矩形放置区域
ROUGH_ZONE = (200, 1450, 580, 150)

# 物料暂存矩形放置区域
TEMP_ZONE = (1620, 1450, 580, 150)

# 精加工预留区域（本流程未使用，仅绘制展示）
FINE_ZONE = (900, 1800, 600, 200)

# 成品存放预留区域（本流程未使用，仅绘制展示）
FINISH_ZONE = (900, 2000, 600, 200)

# 任务二维码识别板固定物理坐标
QR_BOARD_POS = (2100, 1000)


def generate_task_code():
    """
    生成随机四段式比赛任务码，格式：XXX+XXX+XXX+XXX
    分段含义：
    第一段：前3个待抓取物料编号
    第二段：第一批物料对应粗加工放置圆环编号
    第三段：后3个待抓取物料编号
    第四段：第二批物料对应暂存区放置圆环编号
    :return: 任务码字符串, 第一批物料列表, 粗加工环号列表, 第二批物料列表, 暂存环号列表
    """
    # 随机打乱1~6全部物料编号，前3个为第一批，后3个为第二批
    colors = list(range(1, 7))
    random.shuffle(colors)
    batch1_colors, batch2_colors = colors[:3], colors[3:]

    # 随机生成两批1~6圆环编号
    pos1 = [random.randint(1, 6) for _ in range(3)]
    pos2 = [random.randint(1, 6) for _ in range(3)]

    # 拼接标准四段式任务码
    code = "+".join([
        "".join(map(str, batch1_colors)),
        "".join(map(str, pos1)),
        "".join(map(str, batch2_colors)),
        "".join(map(str, pos2))
    ])
    return code, batch1_colors, pos1, batch2_colors, pos2


class CompetitionSimulator:
    """
    仿真核心主类，承载全部仿真逻辑
    功能模块：
    1. 全局状态存储：小车坐标、朝向、携带物料、任务、分数、日志、帧率
    2. 任务调度系统：任务队列、直线插值路径生成、动作延时模拟
    3. 状态机：闲置/运行中/比赛完成三种全局状态
    4. 坐标转换：毫米物理坐标 → 屏幕像素坐标
    5. 渲染模块：全局场地俯视图、车载相机视图、总界面拼接
    """
    def __init__(self):
        """构造函数：实例化类时自动执行，初始化所有仿真状态变量"""
        # 任务码相关存储变量
        self.task_code = ""
        self.batch1_colors = []
        self.batch1_positions = []
        self.batch2_colors = []
        self.batch2_positions = []

        # 机器人小车物理状态
        self.car_pos = np.array([200.0, 200.0]) # 小车浮点坐标[x,y] 单位mm
        self.car_angle = 0                      # 小车朝向角度，0度=水平向右
        self.car_speed = 12.0                   # 每帧小车移动毫米距离
        self.has_material = False               # 布尔标记：是否携带物料
        self.current_material = None            # 当前携带物料编号，无物料则为空

        # 比赛有限状态机控制变量
        self.mission_state = 'idle'     # idle闲置 / running运行中 / completed比赛完成
        self.mission_step = 0           # 当前执行任务队列下标索引
        self.mission_progress = 0       # 任务完成百分比 0~100
        self.total_score = 0            # 本局比赛总得分

        # 小车自动行驶路径变量
        self.path_points = []   # 存储两点间所有插值路径坐标点列表
        self.path_index = 0     # 当前小车走到路径第几个点
        self.wait_counter = 0   # 动作延时计数器，模拟扫码、抓取、放置设备耗时
        self.current_action = ""# 当前执行任务文字描述

        # 模拟机器人串口日志环形缓冲区
        self.serial_log = []
        self.max_log_lines = 10 # 日志最大保存行数，超出自动删除最早日志

        self.obstacles = self._generate_obstacles() # 生成车道随机障碍物坐标
        self.materials_on_platform = []             # 原料转盘上待抓取物料对象列表

        # 加速倍率（1=正常速度，2=2倍速，4=4倍速）
        self.speed_multiplier = 1

        # FPS帧率统计变量
        self.frame_count = 0
        self.last_time = time.time()
        self.fps = 0
        self.anim_frame = 0 # 全局动画总帧计数器

        self.mission_queue = [] # 完整有序任务队列，全自动执行核心容器
        self._field_bg_cache = None  # 场地静态背景缓存，每局重新生成

        self._new_game() # 初始化全新一局比赛

    def _generate_obstacles(self):
        """私有方法：在十字车道随机生成黑色障碍物坐标，返回坐标元组列表"""
        obstacles = []
        # 水平横向车道生成2个障碍
        for _ in range(2):
            x = random.randint(200, 2200)
            y = LANE_CENTER + random.choice([-1, 1]) * random.randint(50, 150)
            obstacles.append((x, y))
        # 垂直纵向车道生成2个障碍
        for _ in range(2):
            x = LANE_CENTER + random.choice([-1, 1]) * random.randint(50, 150)
            y = random.randint(200, 2200)
            obstacles.append((x, y))
        return obstacles

    def _new_game(self):
        """私有方法：重置全部仿真状态，生成新任务、新物料、构建任务队列并启动比赛"""
        # 获取全新随机任务码与物料、放置环配置
        self.task_code, self.batch1_colors, self.batch1_positions, \
            self.batch2_colors, self.batch2_positions = generate_task_code()

        # 初始化原料转盘3个物料，120度均匀分布在圆形转盘上
        self.materials_on_platform = []
        for i, color_id in enumerate(self.batch1_colors):
            angle = i * 120 # 每个物料间隔120度均匀分布
            rad = math.radians(angle) # 角度转弧度，三角函数仅接收弧度参数
            # 三角函数计算物料在转盘上的实际物理mm坐标
            cx = RAW_ZONE_CENTER[0] + 80 * math.cos(rad)
            cy = RAW_ZONE_CENTER[1] + 80 * math.sin(rad)
            color_rgb = COLOR_MAP[color_id][0]
            color_name = COLOR_MAP[color_id][1]
            # 物料完整属性字典存入列表
            self.materials_on_platform.append({
                'pos': (cx, cy),
                'color': color_rgb,
                'label': color_name,
                'color_id': color_id,
                'picked': False # 标记该物料是否已被小车抓取
            })

        self._build_mission_queue() # 按比赛流程构建完整任务队列
        self.start_mission()        # 切换为运行状态，自动开始执行任务

    def _build_mission_queue(self):
        """私有方法：按比赛标准流程顺序构建任务队列字典列表"""
        self.mission_queue = []

        # 步骤1: 从左下角启停区导航行驶到二维码板，仅行驶无设备动作
        self.mission_queue.append({
            'action': 'navigate',
            'target': np.array([QR_BOARD_POS[0], QR_BOARD_POS[1]]),
            'desc': "前往二维码板"
        })

        # 步骤2: 到达二维码点位，执行扫码识别任务
        self.mission_queue.append({
            'action': 'read_qr',
            'target': np.array([QR_BOARD_POS[0], QR_BOARD_POS[1]]),
            'desc': "读取任务码"
        })

        # 步骤3~5：循环3次，依次导航至原料转盘抓取第一批3个物料
        for i in range(3):
            color_id = self.batch1_colors[i]
            color_name = COLOR_MAP[color_id][1]
            # 导航到物料实际位置（转盘上120度分布的点）
            mat_pos = self.materials_on_platform[i]['pos']
            self.mission_queue.append({
                'action': 'navigate_pick', # 导航+抓取物料复合动作
                'target': np.array([mat_pos[0], mat_pos[1]]),
                'desc': f"前往原料区取{color_name}物料",
                'color_id': color_id,
                'material_idx': i # 当前抓取第几个物料，匹配转盘物料下标
            })

        # 步骤6~8：第一批物料依次导航至粗加工区对应圆环放置
        for i in range(3):
            color_id = self.batch1_colors[i]
            pos = self.batch1_positions[i]
            color_name = COLOR_MAP[color_id][1]
            # 计算目标圆环物理mm坐标
            ring_x = ROUGH_ZONE[0] + 60 + pos * 85
            ring_y = ROUGH_ZONE[1] + ROUGH_ZONE[3] // 2
            self.mission_queue.append({
                'action': 'navigate_place_rough',
                'target': np.array([ring_x, ring_y]),
                'desc': f"放置{color_name}到粗加工区{pos}环",
                'ring': pos,
                'color_id': color_id
            })

        # 步骤9~11：第二批物料依次导航至暂存区对应圆环放置
        for i in range(3):
            color_id = self.batch2_colors[i]
            pos = self.batch2_positions[i]
            color_name = COLOR_MAP[color_id][1]
            ring_x = TEMP_ZONE[0] + 60 + pos * 85
            ring_y = TEMP_ZONE[1] + TEMP_ZONE[3] // 2
            self.mission_queue.append({
                'action': 'navigate_place_temp',
                'target': np.array([ring_x, ring_y]),
                'desc': f"放置{color_name}到暂存区{pos}环",
                'ring': pos,
                'color_id': color_id
            })

        # 步骤12：全部物料放置完成，返回左下角起点启停区
        self.mission_queue.append({
            'action': 'return',
            'target': np.array([200.0, 200.0]),
            'desc': "返回启停区"
        })

    def start_mission(self):
        """重置任务运行状态，从头开始顺序执行任务队列"""
        self.mission_state = 'running'
        self.mission_step = 0
        self.mission_progress = 0
        self.total_score = 0
        # 小车复位到左下角起点坐标
        self.car_pos = np.array([200.0, 200.0])
        self.car_angle = 0
        self.has_material = False
        self.current_material = None
        self.wait_counter = 0
        self.serial_log = []
        # 写入开局日志
        self._add_log("=== 比赛开始 ===")
        self._add_log(f"任务码: {self.task_code}")
        self._action_executed = False  # 标记当前步骤动作是否已执行
        self._field_bg_cache = None    # 新对局清除场地背景缓存
        self._set_next_step() # 加载第一个任务的行驶路径

    def _set_next_step(self):
        """私有方法：切换下一个任务步骤，生成起点到目标的插值行驶路径"""
        if self.mission_step < len(self.mission_queue):
            step = self.mission_queue[self.mission_step]
            # 根据小车当前坐标、任务目标坐标生成车道级行驶路径点
            self.path_points = self._compute_path_via_lanes(self.car_pos, step['target'])
            self.path_index = 0
            self.current_action = step['desc']
            self._action_executed = False  # 新步骤，动作未执行
            self._add_log(f"[{self.mission_step+1}/{len(self.mission_queue)}] {step['desc']}")
        else:
            # 所有任务全部执行完毕，标记比赛完成
            self.mission_state = 'completed'
            self.mission_progress = 100
            self._add_log(f"=== 比赛完成！总分: {self.total_score} ===")

    def _compute_path(self, start, end):
        """私有方法：两点之间线性插值生成路径点列表，模拟小车逐帧匀速移动
        :param start: 起点np浮点数组 [x,y] 单位mm
        :param end: 终点np浮点数组 [x,y] 单位mm
        :return: 路径点列表，每个元素为一帧小车坐标
        """
        delta = end - start
        dist = np.linalg.norm(delta)
        steps = max(int(dist / self.car_speed), 1)  # 最少1帧防止除零
        # 向量化线性插值生成每一步坐标，避免Python循环
        ts = np.linspace(0, 1, steps, endpoint=False).reshape(-1, 1)
        points = [start + delta * t for t in ts]
        points.append(end.copy())  # 强制追加终点，避免浮点精度误差
        return points

    def _compute_path_via_lanes(self, start, end):
        """
        车道级路径规划：机器人离开启停区后沿灰色十字车道行驶。
        策略：先接入最近车道，沿水平/垂直车道行驶，再驶出到达目标。
        """
        # 如果起止点都在启停区或原料区内（非车道区域），允许短距离直线驶出/驶入
        in_lane_start = (LANE_START <= start[0] <= LANE_END) or (LANE_START <= start[1] <= LANE_END)
        in_lane_end = (LANE_START <= end[0] <= LANE_END) or (LANE_START <= end[1] <= LANE_END)

        # 如果起止点可通过单条车道直接连接，直接走直线
        if in_lane_start and in_lane_end:
            # 同水平车道
            if LANE_START <= start[1] <= LANE_END and LANE_START <= end[1] <= LANE_END:
                return self._compute_path(start, end)
            # 同垂直车道
            if LANE_START <= start[0] <= LANE_END and LANE_START <= end[0] <= LANE_END:
                return self._compute_path(start, end)

        # 方案A：起点 → 水平车道(y=1200) → 垂直车道(x=1200) → 终点
        p1_a = np.array([start[0], LANE_CENTER])
        p2_a = np.array([LANE_CENTER, end[1]])
        # 方案B：起点 → 垂直车道(x=1200) → 水平车道(y=1200) → 终点
        p1_b = np.array([LANE_CENTER, start[1]])
        p2_b = np.array([end[0], LANE_CENTER])

        def seg_len(a, b):
            return np.linalg.norm(a - b)

        d_a = seg_len(start, p1_a) + seg_len(p1_a, p2_a) + seg_len(p2_a, end)
        d_b = seg_len(start, p1_b) + seg_len(p1_b, p2_b) + seg_len(p2_b, end)

        via = [p1_a, p2_a] if d_a <= d_b else [p1_b, p2_b]

        # 拼接各段路径，移除重复点
        points = []
        current = np.array([start[0], start[1]])
        for p in via:
            if seg_len(current, p) > 1.0:
                segment = self._compute_path(current, p)
                points.extend(segment[:-1])
                current = segment[-1]
        segment = self._compute_path(current, end)
        points.extend(segment)
        return points

    def _add_log(self, msg):
        """私有方法：添加一条模拟串口日志，自动控制日志最大行数"""
        ts = time.strftime("%H:%M:%S") # 获取当前时分秒时间戳
        self.serial_log.append(f"[{ts}] {msg}")
        # 超过最大行数则删除第一条最早日志
        if len(self.serial_log) > self.max_log_lines:
            self.serial_log.pop(0)

    def _calc_ring_score(self, ring):
        """根据圆环编号查询对应得分，无匹配返回0分"""
        return RING_SCORES.get(ring, 0)

    def update(self):
        """每帧核心更新函数：驱动小车移动、动作延时、任务切换，每一帧渲染前调用"""
        # 根据加速倍率执行多帧更新
        for _ in range(self.speed_multiplier):
            self._update_single_frame()

    def _update_single_frame(self):
        """单帧更新逻辑"""
        self.anim_frame += 1
        # 闲置/完成状态不执行运动逻辑
        if self.mission_state != 'running':
            return

        # 动作延时阶段：倒计时，不移动小车
        if self.wait_counter > 0:
            self.wait_counter -= 1
            return

        # 路径未走完：小车向前移动一个路径点
        if self.path_index < len(self.path_points):
            self.car_pos = self.path_points[self.path_index].copy()
            self.path_index += 1
            # 计算小车前进朝向角度
            if self.path_index < len(self.path_points):
                nxt = self.path_points[self.path_index]
                dx = nxt[0] - self.car_pos[0]
                dy = nxt[1] - self.car_pos[1]
                # 坐标存在差值时更新角度，避免静止抖动
                if abs(dx) > 0.1 or abs(dy) > 0.1:
                    self.car_angle = math.degrees(math.atan2(dy, dx))
        elif not self._action_executed:
            # 路径全部走完且动作未执行过，执行当前任务绑定的设备动作（仅执行一次）
            step = self.mission_queue[self.mission_step]
            self._execute_action(step)
            self._action_executed = True
            # 任务下标+1，更新完成进度百分比
            self.mission_step += 1
            self.mission_progress = int((self.mission_step / len(self.mission_queue)) * 100)
            self._set_next_step() # 加载下一个任务路径

    def _execute_action(self, step):
        """私有方法：根据任务action类型执行对应业务逻辑、打印日志、设置延时"""
        if step['action'] == 'read_qr':
            # 扫码动作逻辑
            self._add_log(f"  读取二维码: {self.task_code}")
            self._add_log(f"  CMD:0x09 QR数据接收完成")
            self.wait_counter = 20 # 延时20帧模拟扫码耗时

        elif step['action'] == 'navigate_pick':
            # 抓取物料逻辑
            idx = step.get('material_idx', 0)
            # 对应物料未被抓取时执行抓取标记
            if idx < len(self.materials_on_platform) and not self.materials_on_platform[idx]['picked']:
                self.materials_on_platform[idx]['picked'] = True
                self.has_material = True
                self.current_material = step['color_id']
                color_name = COLOR_MAP[step['color_id']][1]
                self._add_log(f"  抓取: {color_name}物料")
                self._add_log(f"  CMD:0x01 颜色识别完成")
            self.wait_counter = 15 # 抓取延时15帧

        elif step['action'] == 'navigate_place_rough':
            # 粗加工区放置物料、加分逻辑
            ring = step.get('ring', 1)
            score = self._calc_ring_score(ring)
            self.total_score += score
            self.has_material = False
            self.current_material = None
            color_name = COLOR_MAP[step['color_id']][1]
            self._add_log(f"  放置{color_name}到粗加工区{ring}环 (+{score}分)")
            self._add_log(f"  CMD:0x04 码垛定位完成")
            self.wait_counter = 15

        elif step['action'] == 'navigate_place_temp':
            # 暂存区放置物料、加分逻辑
            ring = step.get('ring', 1)
            score = self._calc_ring_score(ring)
            self.total_score += score
            self.has_material = False
            self.current_material = None
            color_name = COLOR_MAP[step['color_id']][1]
            self._add_log(f"  放置{color_name}到暂存区{ring}环 (+{score}分)")
            self._add_log(f"  CMD:0x04 码垛定位完成")
            self.wait_counter = 15

        elif step['action'] == 'return':
            # 返回起点动作日志
            self._add_log(f"  返回启停区，比赛结束")
            self.wait_counter = 20

        elif step['action'] == 'navigate':
            # 纯行驶无动作，短延时
            self.wait_counter = 10

    def _mm_to_px(self, pos_mm):
        """坐标转换工具：物理毫米坐标 → 画布像素坐标
        特殊处理：图像Y轴与物理坐标系上下翻转，屏幕原点左上角，物理原点左下角
        :param pos_mm: (x mm, y mm) 物理坐标元组
        :return: (x像素, y像素) 画布绘图坐标
        """
        x_px = int(pos_mm[0] * PIXEL_PER_MM)
        y_px = int((FIELD_SIZE - pos_mm[1]) * PIXEL_PER_MM)
        return (x_px, y_px)

    def _draw_zone(self, img, zone, color, label):
        """绘制矩形功能区边框与文字标签"""
        x, y, w, h = zone
        sx, sy = self._mm_to_px((x, y))
        ex, ey = self._mm_to_px((x + w, y + h))
        cv2.rectangle(img, (ex, sy), (sx, ey), color, 2)
        cv2.putText(img, label, (ex + 5, sy + 18), cv2.FONT_HERSHEY_SIMPLEX, 0.4, color, 1)

    def _draw_zone_with_rings(self, img, zone, color, label):
        """绘制矩形功能区，并在内部水平排列6个编号圆环"""
        self._draw_zone(img, zone, color, label)
        x, y, w, h = zone
        _, sy = self._mm_to_px((x, y))
        ex, ey = self._mm_to_px((x + w, y + h))
        ry = (sy + ey) // 2
        for i in range(1, 7):
            rx = ex + 20 + i * 80
            cv2.circle(img, (rx, ry), 10, (200, 200, 200), 1)
            cv2.putText(img, str(i), (rx - 3, ry + 3), cv2.FONT_HERSHEY_SIMPLEX, 0.3, (100, 100, 100), 1)

    def _render_field_static(self):
        """绘制静态场地背景（车道、功能区域、障碍物等），结果会被 render_field 缓存复用"""
        size = int(FIELD_SIZE * PIXEL_PER_MM)
        img = np.ones((size, size, 3), dtype=np.uint8) * 245

        # 1. 四角淡黄色安全区域
        for zone in YELLOW_ZONES:
            x, y, w, h = zone
            sx, sy = self._mm_to_px((x, y))
            ex, ey = self._mm_to_px((x + w, y + h))
            cv2.rectangle(img, (ex, sy), (sx, ey), (230, 230, 160), -1)
            cv2.rectangle(img, (ex, sy), (sx, ey), (200, 200, 120), 1)

        # 2. 灰色十字车道
        lane_s = int(LANE_START * PIXEL_PER_MM)
        lane_e = int(LANE_END * PIXEL_PER_MM)
        cv2.rectangle(img, (lane_s, 0), (lane_e, size), (170, 170, 170), -1)
        cv2.rectangle(img, (0, lane_s), (size, lane_e), (170, 170, 170), -1)
        cv2.line(img, (lane_s, 0), (lane_s, size), (120, 120, 120), 1)
        cv2.line(img, (lane_e, 0), (lane_e, size), (120, 120, 120), 1)
        cv2.line(img, (0, lane_s), (size, lane_s), (120, 120, 120), 1)
        cv2.line(img, (0, lane_e), (size, lane_e), (120, 120, 120), 1)

        # 3. 场地最外层黑色粗边框
        cv2.rectangle(img, (0, 0), (size - 1, size - 1), (0, 0, 0), 3)

        # 4. 蓝色机器人启停区域
        for zone in [START_ZONE_1, START_ZONE_2]:
            x, y, w, h = zone
            sx, sy = self._mm_to_px((x, y))
            ex, ey = self._mm_to_px((x + w, y + h))
            cv2.rectangle(img, (ex, sy), (sx, ey), (100, 100, 255), 2)
            cv2.putText(img, "START", (ex + 5, sy + 20), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (100, 100, 255), 1)

        # 5. 原料圆形转盘（不含物料，物料为动态元素）
        cx_mm, cy_mm = RAW_ZONE_CENTER
        cx_px, cy_px = self._mm_to_px((cx_mm, cy_mm))
        r_px = int(RAW_ZONE_RADIUS * PIXEL_PER_MM)
        cv2.circle(img, (cx_px, cy_px), r_px, (220, 220, 220), -1)
        cv2.circle(img, (cx_px, cy_px), r_px, (80, 80, 80), 2)
        cv2.putText(img, "RAW", (cx_px - 15, cy_px - r_px - 8), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 0), 1)

        # 6~9. 加工/存放功能区（复用统一绘制方法）
        self._draw_zone_with_rings(img, ROUGH_ZONE, (180, 140, 0), "ROUGH")
        self._draw_zone_with_rings(img, TEMP_ZONE, (0, 140, 180), "TEMP")
        self._draw_zone(img, FINE_ZONE, (140, 0, 180), "FINE")
        self._draw_zone(img, FINISH_ZONE, (0, 180, 0), "FINISH")

        # 10. 紫色二维码识别板
        qx, qy = QR_BOARD_POS
        qx_px, qy_px = self._mm_to_px((qx, qy))
        cv2.rectangle(img, (qx_px - 20, qy_px - 30), (qx_px + 20, qy_px + 30), (128, 0, 128), 2)
        cv2.putText(img, "QR", (qx_px - 8, qy_px + 5), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (128, 0, 128), 1)

        # 11. 车道黑色障碍物圆点（每局随机生成，但对局内固定，放入静态缓存）
        for ox, oy in self.obstacles:
            ox_px, oy_px = self._mm_to_px((ox, oy))
            r = int(25 * PIXEL_PER_MM)
            cv2.circle(img, (ox_px, oy_px), r, (20, 20, 20), -1)
            cv2.circle(img, (ox_px, oy_px), r, (0, 0, 0), 1)

        return img

    def render_field(self):
        """绘制全局2400mm场地俯视图：复用静态背景缓存，仅叠加每帧动态元素"""
        # 首次或新对局后重新生成静态背景
        if self._field_bg_cache is None:
            self._field_bg_cache = self._render_field_static()
        # 复制背景，避免破坏缓存
        img = self._field_bg_cache.copy()

        # 动态元素1：转盘上未被抓取的物料圆点
        for mat in self.materials_on_platform:
            if mat['picked']:
                continue
            mx, my = mat['pos']
            mx_px, my_px = self._mm_to_px((mx, my))
            cv2.circle(img, (mx_px, my_px), int(18 * PIXEL_PER_MM), mat['color'], -1)
            cv2.circle(img, (mx_px, my_px), int(18 * PIXEL_PER_MM), (0, 0, 0), 1)

        # 动态元素2：小车当前规划行驶路径浅蓝色线条
        if len(self.path_points) > 0 and self.path_index < len(self.path_points):
            pts = []
            for i in range(self.path_index, min(len(self.path_points), self.path_index + 80)):
                p = self.path_points[i]
                px, py = self._mm_to_px(p)
                pts.append([px, py])
            if len(pts) > 1:
                pts = np.array(pts, np.int32)
                cv2.polylines(img, [pts], False, (0, 150, 255), 2, cv2.LINE_AA)

        # 动态元素3：三角形小车本体（预计算 cos/sin，避免重复三角函数调用）
        car_px, car_py = self._mm_to_px(self.car_pos)
        car_size = int(35 * PIXEL_PER_MM)
        angle_rad = math.radians(self.car_angle)
        cos_a, sin_a = math.cos(angle_rad), math.sin(angle_rad)
        body_pts = np.array([
            [car_px + car_size * cos_a, car_py - car_size * sin_a],
            [car_px + car_size * 0.5 * math.cos(angle_rad + 2.6), car_py - car_size * 0.5 * math.sin(angle_rad + 2.6)],
            [car_px + car_size * 0.5 * math.cos(angle_rad - 2.6), car_py - car_size * 0.5 * math.sin(angle_rad - 2.6)],
        ], np.int32)
        cv2.fillPoly(img, [body_pts], (0, 0, 220))
        cv2.polylines(img, [body_pts], True, (0, 0, 0), 1)

        # 小车上方绿色任务码显示屏
        disp_y = car_py - car_size - 6
        cv2.rectangle(img, (car_px - 15, disp_y - 8), (car_px + 15, disp_y + 4), (0, 255, 0), -1)
        cv2.rectangle(img, (car_px - 15, disp_y - 8), (car_px + 15, disp_y + 4), (0, 0, 0), 1)

        # 小车携带物料圆点（车身中心）
        if self.has_material and self.current_material:
            cv2.circle(img, (car_px, car_py), int(6 * PIXEL_PER_MM), COLOR_MAP[self.current_material][0], -1)
            cv2.circle(img, (car_px, car_py), int(6 * PIXEL_PER_MM), (0, 0, 0), 1)

        return img

    def render_camera_view(self):
        """渲染车载相机仿真小窗口画面，根据当前任务切换不同预览界面"""
        # 创建420*300黑色背景画布
        img = np.zeros((300, 420, 3), dtype=np.uint8)
        cv2.rectangle(img, (0, 0), (420, 300), (30, 30, 30), -1)

        # 获取当前正在执行的任务步骤
        step = self.mission_queue[self.mission_step] if self.mission_step < len(self.mission_queue) else None

        # 场景1：扫码任务相机画面
        if step and step['action'] == 'read_qr':
            cv2.putText(img, "QR Code Reading (unit=9)", (60, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
            qr_size = 120
            qx = (420 - qr_size) // 2
            qy = (300 - qr_size) // 2
            cv2.rectangle(img, (qx, qy), (qx + qr_size, qy + qr_size), (255, 255, 255), 2)
            # 绘制模拟黑白二维码方块
            for i in range(0, qr_size, 12):
                for j in range(0, qr_size, 12):
                    if ((i // 12) + (j // 12)) % 3 == 0:
                        cv2.rectangle(img, (qx + i, qy + j), (qx + i + 8, qy + j + 8), (0, 0, 0), -1)
            # 绿色识别框
            pts = np.array([[qx - 8, qy - 8], [qx + qr_size + 8, qy - 8],
                           [qx + qr_size + 8, qy + qr_size + 8], [qx - 8, qy + qr_size + 8]], np.int32)
            cv2.polylines(img, [pts], True, (0, 255, 0), 2)
            cv2.putText(img, f"Task: {self.task_code}", (80, 280), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)

        # 场景2：抓取物料识别画面
        elif step and step['action'] == 'navigate_pick':
            cv2.putText(img, "Material Detection (unit=1)", (50, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
            positions = [(100, 150), (210, 150), (320, 150)]
            for i, mat in enumerate(self.materials_on_platform):
                if mat['picked']:
                    # 已抓取物料灰色变暗，标注TAKEN
                    cv2.circle(img, positions[i], 30, (60, 60, 60), -1)
                    cv2.putText(img, "TAKEN", (positions[i][0] - 20, positions[i][1] + 5), cv2.FONT_HERSHEY_SIMPLEX, 0.3, (150, 150, 150), 1)
                else:
                    cv2.circle(img, positions[i], 30, mat['color'], -1)
                    cv2.circle(img, positions[i], 30, (255, 255, 255), 2)
                    cv2.putText(img, mat['label'], (positions[i][0] - 15, positions[i][1] + 5), cv2.FONT_HERSHEY_SIMPLEX, 0.35, (255, 255, 255), 1)
            # 目标物料青色选中框
            target_idx = step.get('material_idx', 0)
            if target_idx < len(positions):
                cv2.rectangle(img, (positions[target_idx][0] - 35, positions[target_idx][1] - 35),
                             (positions[target_idx][0] + 35, positions[target_idx][1] + 35), (0, 255, 255), 2)
            cv2.putText(img, f"CMD: 0x01 | Target: {COLOR_MAP[step['color_id']][1]}", (10, 280), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1)

        # 场景3：粗加工/暂存区放置圆环识别画面
        elif step and step['action'] in ('navigate_place_rough', 'navigate_place_temp'):
            zone_name = "Rough" if step['action'] == 'navigate_place_rough' else "Temp"
            ring = step.get('ring', 1)
            cv2.putText(img, f"{zone_name} Zone Placement (unit=4)", (30, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
            for i in range(1, 7):
                rx = 40 + i * 55
                ry = 150
                # 目标圆环亮绿色高亮，其余灰色
                color = (0, 255, 0) if i == ring else (100, 100, 100)
                cv2.circle(img, (rx, ry), 18, color, 2)
                cv2.putText(img, str(i), (rx - 4, ry + 4), cv2.FONT_HERSHEY_SIMPLEX, 0.4, color, 1)
            cv2.putText(img, f"Ring {ring} | Score: +{self._calc_ring_score(ring)}", (10, 280), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1)

        # 场景4：返回起点导航画面
        elif step and step['action'] == 'return':
            cv2.putText(img, "Returning to Start Zone", (80, 150), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)

        # 默认场景：普通行驶导航模式
        else:
            cv2.putText(img, "Navigation Mode", (130, 150), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (200, 200, 200), 1)

        return img

    def get_frame(self):
        """
        总画面合成主函数
        每一帧渲染都会调用，执行逻辑：
        1. 执行一帧仿真更新（小车移动、任务切换、动作计时）
        2. 创建1280×720总深色画布
        3. 拼接4大模块：左侧场地俯视图、右上车载相机、右侧任务面板、底部状态栏
        4. 返回完整合成画布，供视频流编码推送至网页
        """
        # 先执行一帧仿真更新逻辑，刷新小车、任务、日志状态
        self.update()

        # 定义总画布固定分辨率 宽1280 高720
        canvas_w = 1280
        canvas_h = 720
        # 创建黑色画布数组，3通道RGB
        canvas = np.zeros((canvas_h, canvas_w, 3), dtype=np.uint8)
        # 填充深灰底色 (R=20,G=20,B=30)
        canvas[:] = (20, 20, 30)

        # ====================== 左侧：场地俯视图 ======================
        # 调用场地绘制函数，获取完整场地图像
        field_img = self.render_field()
        # 提取图像高度、宽度
        fh, fw = field_img.shape[:2]
        # 等比例缩放，限制最大宽度620像素，防止占满界面
        scale = min(620 / fw, 620 / fh)
        # 计算缩放后宽高
        nw = int(fw * scale)
        nh = int(fh * scale)
        # 执行图像缩放
        field_img = cv2.resize(field_img, (nw, nh))
        # 将缩放后的场地图粘贴到大画布：起始坐标x=15,y=45
        canvas[45:45 + nh, 15:15 + nw] = field_img
        # 在画布左上角绘制中文标题
        canvas = put_chinese_text(canvas, "【场地俯视图 2400x2400mm】", (15, 10), font_size=16, color=(0, 255, 255))

        # ====================== 右上角：车载相机仿真窗口 ======================
        # 生成当前任务对应的相机预览画面
        cam_img = self.render_camera_view()
        # 粘贴到画布右上区域 x560,y45 宽420高300
        canvas[45:345, 560:980] = cam_img
        # 绘制相机窗口标题
        canvas = put_chinese_text(canvas, "【摄像头视角】", (560, 10), font_size=16, color=(0, 255, 255))

        # ====================== 右侧中部：任务信息面板 ======================
        info_x = 560    # 信息面板起始X坐标
        info_y = 365    # 信息面板起始Y坐标
        # 绘制板块标题
        canvas = put_chinese_text(canvas, "【任务信息】", (info_x, info_y - 20), font_size=16, color=(0, 255, 255))

        # 打印完整四段式任务码，绿色加粗字体
        cv2.putText(canvas, f"Task: {self.task_code}", (info_x, info_y + 25), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)

        # 拼接第一批物料+粗加工环字符串
        batch1_str = "B1: " + "".join([COLOR_MAP[c][1][:3] for c in self.batch1_colors]) + \
                    " -> R:" + "".join([str(p) for p in self.batch1_positions])
        # 拼接第二批物料+暂存环字符串
        batch2_str = "B2: " + "".join([COLOR_MAP[c][1][:3] for c in self.batch2_colors]) + \
                    " -> T:" + "".join([str(p) for p in self.batch2_positions])
        # 绘制两行批次信息，浅灰色小字
        cv2.putText(canvas, batch1_str, (info_x, info_y + 50), cv2.FONT_HERSHEY_SIMPLEX, 0.38, (200, 200, 200), 1)
        cv2.putText(canvas, batch2_str, (info_x, info_y + 72), cv2.FONT_HERSHEY_SIMPLEX, 0.38, (200, 200, 200), 1)

        # 绘制任务进度条
        bar_y = info_y + 90
        # 进度条灰色背景底板
        cv2.rectangle(canvas, (info_x, bar_y), (info_x + 300, bar_y + 18), (50, 50, 50), -1)
        # 计算已完成进度宽度
        bar_w = int(300 * self.mission_progress / 100)
        # 比赛完成=绿色进度条，进行中=青色进度条
        bar_color = (0, 255, 0) if self.mission_state == 'completed' else (0, 165, 255)
        # 填充进度条
        cv2.rectangle(canvas, (info_x, bar_y), (info_x + bar_w, bar_y + 18), bar_color, -1)
        # 进度百分比+总分文字
        cv2.putText(canvas, f"{self.mission_progress}%  Score: {self.total_score}", (info_x + 310, bar_y + 14), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)

        # 打印当前执行动作英文
        cv2.putText(canvas, f"Action: {self.current_action}", (info_x, bar_y + 40), cv2.FONT_HERSHEY_SIMPLEX, 0.42, (0, 255, 255), 1)
        # 打印当前执行动作中文
        canvas = put_chinese_text(canvas, f"当前动作: {self.current_action}", (info_x, bar_y + 40), font_size=13, color=(0, 255, 255))

        # 绘制前后任务列表预览，最多展示7行任务
        step_y = bar_y + 60
        total_steps = len(self.mission_queue)
        display_steps = min(total_steps, 7)
        # 从当前任务往前取2条历史任务，方便查看进度
        start_idx = max(0, self.mission_step - 2)
        # 循环渲染每条任务
        for i in range(start_idx, min(start_idx + display_steps, total_steps)):
            s = self.mission_queue[i]
            # 已完成任务：绿色[OK]标记
            if i < self.mission_step:
                mark = "[OK]"
                c = (0, 255, 0)
            # 当前正在执行任务：青色[>>]标记
            elif i == self.mission_step and self.mission_state == 'running':
                mark = "[>>]"
                c = (0, 255, 255)
            # 未执行任务：灰色空白标记
            else:
                mark = "[  ]"
                c = (80, 80, 80)
            # 绘制单行任务文本
            canvas = put_chinese_text(canvas, f"{mark} {s['desc']}", (info_x, step_y), font_size=13, color=c)
            # 行间距20像素
            step_y += 20

        # ====================== 通信日志区域 ======================
        log_x = 560
        log_y = 560
        # 日志板块标题
        canvas = put_chinese_text(canvas, "【通信日志】", (log_x, log_y - 20), font_size=16, color=(0, 255, 255))
        log_y += 18
        # 循环打印每条模拟串口日志，绿色小字
        for log in self.serial_log:
            canvas = put_chinese_text(canvas, log[:65], (log_x, log_y), font_size=12, color=(0, 255, 0))
            log_y += 16

        # ====================== 底部全局状态栏 ======================
        canvas = put_chinese_text(canvas, f"位置: ({int(self.car_pos[0])},{int(self.car_pos[1])})mm  角度: {int(self.car_angle)}deg  FPS: {self.fps}",
                (15, 700), font_size=13, color=(180, 180, 180))

        # 返回合成完成的一帧完整画面
        return canvas


# ====================== 全局仿真对象实例化 ======================
# 创建全局唯一仿真实例，所有网页接口共用这一套仿真状态
simulator = CompetitionSimulator()


# ====================== Flask网页路由接口 ======================
@app.route('/')
def index():
    """
    首页根路由：访问 127.0.0.1:5000 触发
    作用：加载templates文件夹下index.html前端页面，页面包含视频播放器
    """
    return render_template('index.html')


@app.route('/video_feed')
def video_feed():
    """
    实时视频流接口，页面img标签src指向该地址
    使用MJPG流式输出，持续不断返回每一帧jpg图像
    """
    def generate():
        # 无限循环持续生成画面帧
        while True:
            # 获取合成完整的一帧画面
            frame = simulator.get_frame()
            # 帧计数+1，用于每秒统计FPS
            simulator.frame_count += 1
            # 获取当前系统时间
            now = time.time()
            # 每隔1秒计算一次帧率
            if now - simulator.last_time >= 1.0:
                # 1秒内渲染总帧数即为FPS
                simulator.fps = simulator.frame_count
                # 重置帧计数器
                simulator.frame_count = 0
                # 重置计时起点
                simulator.last_time = now

            # 将OpenCV图像编码为jpg二进制流
            _, buffer = cv2.imencode('.jpg', frame)
            # 按照multipart视频流协议分段返回二进制图片
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')
            # 每帧延时0.05秒，限制最高20帧，降低CPU占用
            time.sleep(0.05)

    # 返回持续流式响应，指定媒体类型为分段图片流
    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=frame')


@app.route('/restart')
def restart():
    """
    重置对局接口，前端按钮调用
    作用：清空当前所有仿真状态，生成全新随机任务，重启比赛
    返回JSON成功标识与新任务码
    """
    # 调用内部方法重置全局仿真实例
    simulator._new_game()
    # 返回json格式响应
    return jsonify({'status': 'success', 'message': '新比赛已开始', 'task_code': simulator.task_code})


@app.route('/set_speed')
def set_speed():
    """设置仿真加速倍率"""
    speed = int(request.args.get('speed', 1))
    if speed in [1, 2, 4, 8]:
        simulator.speed_multiplier = speed
        return jsonify({'status': 'success', 'speed': speed})
    return jsonify({'status': 'error', 'message': '无效速度'})


@app.route('/get_info')
def get_info():
    """
    全量仿真状态查询接口
    前端JS定时调用，获取所有实时仿真数据，用于前端独立面板展示
    返回完整JSON数据包，包含任务、小车、分数、日志、帧率全部信息
    """
    # 组装所有仿真状态并转为JSON返回
    return jsonify({
        'task_code': simulator.task_code,                          # 全局任务码
        'batch1_colors': [COLOR_MAP[c][1] for c in simulator.batch1_colors], # 第一批物料名称
        'batch1_positions': simulator.batch1_positions,            # 粗加工放置环号
        'batch2_colors': [COLOR_MAP[c][1] for c in simulator.batch2_colors], # 第二批物料名称
        'batch2_positions': simulator.batch2_positions,            # 暂存放置环号
        'mission_state': simulator.mission_state,                  # 比赛状态 idle/running/completed
        'mission_step': simulator.mission_step,                     # 当前执行任务下标
        'mission_progress': simulator.mission_progress,             # 任务完成百分比0~100
        'total_score': simulator.total_score,                       # 本局总得分
        'current_task': simulator.current_action,                   # 当前动作文字（直接使用内部状态）
        'car_pos': [int(simulator.car_pos[0]), int(simulator.car_pos[1])], # 小车坐标mm
        'car_angle': int(simulator.car_angle),                      # 小车朝向角度
        'has_material': simulator.has_material,                    # 是否携带物料布尔值
        'fps': simulator.fps,                                      # 实时帧率
        'serial_log': simulator.serial_log,                        # 串口日志列表
        'ring_scores': RING_SCORES                                 # 圆环得分规则
    })


# ====================== 程序入口判断 ======================
if __name__ == '__main__':
    """
    仅直接运行本脚本时执行，导入为模块时不启动服务
    host=0.0.0.0：允许局域网所有设备访问网页
    port=5000：监听5000端口
    debug=True：开启热重载、网页报错面板（仅开发调试使用）
    """
    app.run(host='0.0.0.0', port=5000, debug=True)