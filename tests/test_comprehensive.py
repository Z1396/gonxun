#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
综合测试单元程序
测试物料色块、圆环、二维码、黑色障碍物四大模块
自动生成测试图像并验证检测效果
"""
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import cv2
import numpy as np

from vision import (
    ColorDetector,
    ThreeRingDetector,
    SixRingDetector,
    QRDetector,
    ObstacleDetector,
    TaskCodeParser
)


class TestImageGenerator:
    """测试图像生成器"""

    @staticmethod
    def create_color_blocks_image():
        """创建三色物料色块测试图"""
        img = np.zeros((480, 640, 3), dtype=np.uint8)
        img[:] = (240, 240, 240)

        # 红色物料块 (圆心 200, 200, 半径 50)
        cv2.circle(img, (200, 200), 50, (0, 0, 255), -1)
        # 绿色物料块 (圆心 320, 200, 半径 50)
        cv2.circle(img, (320, 200), 50, (0, 255, 0), -1)
        # 蓝色物料块 (圆心 440, 200, 半径 50)
        cv2.circle(img, (440, 200), 50, (255, 0, 0), -1)

        return img

    @staticmethod
    def create_three_rings_image():
        """创建三环定位测试图"""
        img = np.zeros((480, 640, 3), dtype=np.uint8)
        img[:] = (200, 200, 200)

        # 三个空心色环
        colors = [(0, 0, 255), (0, 255, 0), (255, 0, 0)]
        positions = [(180, 240), (320, 240), (460, 240)]

        for color, pos in zip(colors, positions):
            cv2.circle(img, pos, 40, color, 8)

        return img

    @staticmethod
    def create_six_rings_image():
        """创建六环评分测试图"""
        img = np.zeros((400, 800, 3), dtype=np.uint8)
        img[:] = (255, 255, 255)

        # 6个圆环，不同半径
        for i in range(6):
            radius = 20 + i * 5
            cv2.circle(img, (80 + i * 130, 200), radius, (0, 0, 0), 2)

        return img

    @staticmethod
    def create_qr_code_image():
        """创建二维码测试图"""
        import qrcode

        qr = qrcode.QRCode(version=1, box_size=10, border=5)
        qr.add_data('156+123+516+231')
        qr.make(fit=True)

        qr_img = qr.make_image(fill_color="black", back_color="white")
        qr_img = np.array(qr_img.convert('RGB'))
        qr_img = cv2.resize(qr_img, (200, 200))

        img = np.ones((480, 640, 3), dtype=np.uint8) * 255
        img[140:340, 220:420] = qr_img

        return img

    @staticmethod
    def create_obstacle_image():
        """创建黑色障碍物测试图"""
        img = np.zeros((480, 640, 3), dtype=np.uint8)
        img[:] = (200, 200, 200)

        # 两个黑色圆形障碍物
        cv2.circle(img, (200, 240), 25, (0, 0, 0), -1)
        cv2.circle(img, (440, 240), 25, (0, 0, 0), -1)

        return img


class TestRunner:
    """测试运行器"""

    def __init__(self):
        self.passed = 0
        self.failed = 0

    def log_pass(self, test_name, detail=""):
        self.passed += 1
        print(f"  [PASS] {test_name} {detail}")

    def log_fail(self, test_name, detail=""):
        self.failed += 1
        print(f"  [FAIL] {test_name} {detail}")

    def run_color_test(self):
        """测试物料色块检测"""
        print("\n" + "=" * 60)
        print("【测试1】物料色块检测 (ColorDetector)")
        print("=" * 60)

        detector = ColorDetector()
        img = TestImageGenerator.create_color_blocks_image()

        # 测试红色检测
        red_pos = detector.detect(img, 'red', 2000)
        if red_pos and abs(red_pos[0] - 200) < 20 and abs(red_pos[1] - 200) < 20:
            self.log_pass("红色物料检测", f"位置({red_pos[0]}, {red_pos[1]})")
        else:
            self.log_fail("红色物料检测", f"期望(200,200) 实际{red_pos}")

        # 测试绿色检测
        green_pos = detector.detect(img, 'green', 2000)
        if green_pos and abs(green_pos[0] - 320) < 20 and abs(green_pos[1] - 200) < 20:
            self.log_pass("绿色物料检测", f"位置({green_pos[0]}, {green_pos[1]})")
        else:
            self.log_fail("绿色物料检测", f"期望(320,200) 实际{green_pos}")

        # 测试蓝色检测
        blue_pos = detector.detect(img, 'blue', 2000)
        if blue_pos and abs(blue_pos[0] - 440) < 20 and abs(blue_pos[1] - 200) < 20:
            self.log_pass("蓝色物料检测", f"位置({blue_pos[0]}, {blue_pos[1]})")
        else:
            self.log_fail("蓝色物料检测", f"期望(440,200) 实际{blue_pos}")

        # 测试不存在的颜色
        none_pos = detector.detect(img, 'purple', 2000)
        if none_pos is None:
            self.log_pass("不存在颜色返回None")
        else:
            self.log_fail("不存在颜色返回None", f"实际返回{none_pos}")

        # 测试空图像
        empty_pos = detector.detect(None, 'red', 2000)
        if empty_pos is None:
            self.log_pass("空图像返回None")
        else:
            self.log_fail("空图像返回None", f"实际返回{empty_pos}")

    def run_ring_test(self):
        """测试圆环检测"""
        print("\n" + "=" * 60)
        print("【测试2】圆环检测 (RingDetector)")
        print("=" * 60)

        # 三环检测
        print("\n--- 三环定位检测 ---")
        three_detector = ThreeRingDetector()
        three_img = TestImageGenerator.create_three_rings_image()
        three_result = three_detector.detect(three_img)

        if three_result and len(three_result) == 6:
            self.log_pass("三环检测", f"返回6个坐标值")
        else:
            self.log_fail("三环检测", f"期望6个坐标 实际{three_result}")

        # 六环检测
        print("\n--- 六环评分检测 ---")
        six_detector = SixRingDetector()
        six_img = TestImageGenerator.create_six_rings_image()
        six_result = six_detector.detect(six_img)

        if six_result and len(six_result) == 6:
            self.log_pass("六环检测", f"检测到{len(six_result)}个环")
        else:
            self.log_fail("六环检测", f"期望6个环 实际{six_result}")

        # 评分计算测试
        print("\n--- 评分计算 ---")
        from vision import calc_placement_score

        test_scores = [
            (1, False, 15),
            (2, False, 10),
            (3, False, 7),
            (4, False, 5),
            (5, False, 3),
            (6, False, 1),
            (None, False, 0),
            (1, True, 0)
        ]

        all_ok = True
        for ring_id, fallen, expected in test_scores:
            actual = calc_placement_score(ring_id, fallen)
            if actual != expected:
                self.log_fail(f"评分计算 环{ring_id} 倾倒{fallen}",
                            f"期望{expected} 实际{actual}")
                all_ok = False

        if all_ok:
            self.log_pass("评分计算", "全部场景正确")

    def run_qr_test(self):
        """测试二维码检测"""
        print("\n" + "=" * 60)
        print("【测试3】二维码检测 (QRDetector)")
        print("=" * 60)

        qr_detector = QRDetector()
        qr_img = TestImageGenerator.create_qr_code_image()

        # 二维码识别
        qr_data = qr_detector.detect(qr_img)
        if qr_data and '156+123+516+231' in qr_data:
            self.log_pass("二维码识别", f"内容: {qr_data}")
        else:
            self.log_fail("二维码识别", f"期望包含任务码 实际{qr_data}")

        # 任务码解析
        print("\n--- 任务码解析 ---")
        parser = TaskCodeParser()

        # 标准格式
        result = parser.parse("156+123+516+231")
        expected = ([1, 5, 6], [1, 2, 3], [5, 1, 6], [2, 3, 1])
        if result == expected:
            self.log_pass("标准格式解析", f"结果: {result}")
        else:
            self.log_fail("标准格式解析", f"期望{expected} 实际{result}")

        # 错误格式拒绝
        bad_cases = ["123+456+789", "1a2+123+123+123", "", None]
        all_rejected = True
        for bad in bad_cases:
            if parser.parse(bad) is not None:
                self.log_fail(f"错误格式拒绝 '{bad}'", "未被拒绝")
                all_rejected = False

        if all_rejected:
            self.log_pass("错误格式拒绝", "全部正确拒绝")

        # 空图像
        empty_result = qr_detector.detect(None)
        if empty_result is None:
            self.log_pass("空图像返回None")
        else:
            self.log_fail("空图像返回None", f"实际返回{empty_result}")

    def run_obstacle_test(self):
        """测试黑色障碍物检测"""
        print("\n" + "=" * 60)
        print("【测试4】黑色障碍物检测 (ObstacleDetector)")
        print("=" * 60)

        detector = ObstacleDetector()
        obs_img = TestImageGenerator.create_obstacle_image()

        # 障碍物检测
        obstacles = detector.detect(obs_img)
        if len(obstacles) >= 1:
            self.log_pass("障碍物检测", f"检测到{len(obstacles)}个障碍物")
        else:
            self.log_fail("障碍物检测", f"期望>=1个 实际{len(obstacles)}")

        # 检测并绘图
        draw_img = obs_img.copy()
        drawn_obstacles = detector.detect_and_draw(draw_img)
        if len(drawn_obstacles) >= 1:
            self.log_pass("检测并绘图", f"绘制{len(drawn_obstacles)}个标记")
        else:
            self.log_fail("检测并绘图", f"绘制失败")

        # 空图像
        empty_obs = detector.detect(None)
        if empty_obs == []:
            self.log_pass("空图像返回空列表")
        else:
            self.log_fail("空图像返回空列表", f"实际返回{empty_obs}")

        # 无障碍物图像
        no_obs_img = np.ones((480, 640, 3), dtype=np.uint8) * 255
        no_obs = detector.detect(no_obs_img)
        if len(no_obs) == 0:
            self.log_pass("无障碍物返回空列表")
        else:
            self.log_fail("无障碍物返回空列表", f"实际检测到{len(no_obs)}个")

    def run_all(self):
        """运行全部测试"""
        print("=" * 60)
        print("工创赛2025视觉系统 - 综合测试单元")
        print("=" * 60)

        self.run_color_test()
        self.run_ring_test()
        self.run_qr_test()
        self.run_obstacle_test()

        # 汇总
        print("\n" + "=" * 60)
        print(f"测试结果汇总")
        print("=" * 60)
        print(f"通过: {self.passed}")
        print(f"失败: {self.failed}")
        print(f"总计: {self.passed + self.failed}")
        print(f"通过率: {self.passed / (self.passed + self.failed) * 100:.1f}%" if (self.passed + self.failed) > 0 else "")
        print("=" * 60)

        return self.failed == 0


def main():
    """程序入口"""
    runner = TestRunner()
    success = runner.run_all()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
