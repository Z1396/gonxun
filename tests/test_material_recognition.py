#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
物料识别系统测试用例
测试内容：
1. ColorDetector 单元测试
2. MaterialRecognizer 接口测试
3. 融合逻辑测试
4. 识别准确率测试（模拟数据）
"""
import sys
import os
import unittest
import numpy as np
import cv2

# 添加项目根目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from vision.color_detector import ColorDetector, COLOR_DIST, COLOR_ID_MAP
from vision.material_recognizer import MaterialRecognizer, MaterialResult, YOLO_AVAILABLE


class TestColorDetector(unittest.TestCase):
    """颜色检测器单元测试"""

    def setUp(self):
        self.detector = ColorDetector()

    def _create_color_image(self, color_name, size=(480, 640)):
        """创建纯色测试图像"""
        h, w = size
        if color_name == 'red':
            bgr = (0, 0, 255)
        elif color_name == 'green':
            bgr = (0, 255, 0)
        elif color_name == 'blue':
            bgr = (255, 0, 0)
        elif color_name == 'yellow':
            bgr = (0, 255, 255)
        elif color_name == 'black':
            bgr = (0, 0, 0)
        elif color_name == 'light_blue':
            bgr = (180, 180, 100)  # HSV=(90,113,180) 在阈值范围内
        else:
            bgr = (128, 128, 128)
        return np.full((h, w, 3), bgr, dtype=np.uint8)

    def test_detect_red(self):
        """测试红色检测"""
        img = self._create_color_image('red')
        pos = self.detector.detect(img, 'red', min_area=100, max_area=1000000)
        self.assertIsNotNone(pos, "应检测到红色色块")
        if pos:
            self.assertIsInstance(pos[0], int)
            self.assertIsInstance(pos[1], int)

    def test_detect_blue(self):
        """测试蓝色检测"""
        img = self._create_color_image('blue')
        pos = self.detector.detect(img, 'blue', min_area=100, max_area=1000000)
        self.assertIsNotNone(pos, "应检测到蓝色色块")

    def test_detect_green(self):
        """测试绿色检测"""
        img = self._create_color_image('green')
        pos = self.detector.detect(img, 'green', min_area=100, max_area=1000000)
        self.assertIsNotNone(pos, "应检测到绿色色块")

    def test_detect_yellow(self):
        """测试黄色检测"""
        img = self._create_color_image('yellow')
        pos = self.detector.detect(img, 'yellow', min_area=100, max_area=1000000)
        self.assertIsNotNone(pos, "应检测到黄色色块")

    def test_detect_black(self):
        """测试黑色检测"""
        img = self._create_color_image('black')
        pos = self.detector.detect(img, 'black', min_area=100, max_area=1000000)
        self.assertIsNotNone(pos, "应检测到黑色色块")

    def test_detect_light_blue(self):
        """测试浅蓝色检测"""
        img = self._create_color_image('light_blue')
        pos = self.detector.detect(img, 'light_blue', min_area=100, max_area=1000000)
        self.assertIsNotNone(pos, "应检测到浅蓝色色块")

    def test_detect_none_image(self):
        """测试空图像输入"""
        pos = self.detector.detect(None, 'red')
        self.assertIsNone(pos)

    def test_detect_invalid_color(self):
        """测试无效颜色"""
        img = self._create_color_image('red')
        pos = self.detector.detect(img, 'purple')
        self.assertIsNone(pos)

    def test_detect_area_too_small(self):
        """测试面积过小过滤"""
        img = np.zeros((480, 640, 3), dtype=np.uint8)
        # 绘制小色块
        cv2.rectangle(img, (300, 200), (310, 210), (255, 0, 0), -1)
        pos = self.detector.detect(img, 'blue', min_area=5000)
        self.assertIsNone(pos, "小面积色块应被过滤")

    def test_color_id_map(self):
        """测试颜色编号映射"""
        for cid in range(1, 7):
            key, name = COLOR_ID_MAP[cid]
            self.assertIn(key, COLOR_DIST)
            self.assertIsNotNone(name)

    def test_get_color_by_id(self):
        """测试根据ID获取颜色"""
        from vision.color_detector import get_color_by_id
        key, name = get_color_by_id(3)
        self.assertEqual(key, 'blue')
        self.assertEqual(name, '蓝色')

        key, name = get_color_by_id(99)
        self.assertIsNone(key)


@unittest.skipIf(not YOLO_AVAILABLE, "ultralytics未安装，跳过YOLO相关测试")
class TestMaterialRecognizer(unittest.TestCase):
    """物料识别器测试"""

    def setUp(self):
        self.recognizer = MaterialRecognizer(
            yolo_model='yolov8n.pt', conf_threshold=0.25
        )

    def test_recognizer_init(self):
        """测试识别器初始化"""
        self.assertIsNotNone(self.recognizer.yolo)
        self.assertIsNotNone(self.recognizer.color_detector)

    def test_recognize_none_image(self):
        """测试空图像输入"""
        results = self.recognizer.recognize(None)
        self.assertEqual(results, [])

    def test_recognize_blank_image(self):
        """测试空白图像"""
        img = np.zeros((480, 640, 3), dtype=np.uint8)
        results = self.recognizer.recognize(img)
        self.assertIsInstance(results, list)

    def test_material_result_label(self):
        """测试结果标签生成"""
        res = MaterialResult(
            yolo_bbox=(10, 20, 100, 200), yolo_conf=0.8,
            yolo_class='bottle', color_name='blue', color_id=3
        )
        self.assertEqual(res.label, 'bottle(blue)')
        self.assertTrue(res.has_color)

        res2 = MaterialResult(
            yolo_bbox=(10, 20, 100, 200), yolo_conf=0.8,
            yolo_class='bottle'
        )
        self.assertEqual(res2.label, 'bottle')
        self.assertFalse(res2.has_color)


class TestColorAnalysisAccuracy(unittest.TestCase):
    """颜色分析准确率测试"""

    def setUp(self):
        self.detector = ColorDetector()

    def _create_test_image_with_block(self, color_bgr, block_size=(100, 100),
                                      img_size=(480, 640)):
        """创建带色块的测试图像"""
        img = np.zeros((*img_size, 3), dtype=np.uint8)
        h, w = img_size
        bh, bw = block_size
        x1 = (w - bw) // 2
        y1 = (h - bh) // 2
        cv2.rectangle(img, (x1, y1), (x1 + bw, y1 + bh), color_bgr, -1)
        return img

    def test_blue_block_detection(self):
        """测试蓝色色块检测准确率"""
        img = self._create_test_image_with_block((255, 0, 0), (150, 150))
        pos = self.detector.detect(img, 'blue', min_area=500, max_area=1000000)
        self.assertIsNotNone(pos, "应检测到蓝色色块")
        if pos:
            # 中心点应在图像中心附近
            self.assertAlmostEqual(pos[0], 320, delta=30)
            self.assertAlmostEqual(pos[1], 240, delta=30)

    def test_red_block_detection(self):
        """测试红色色块检测准确率"""
        img = self._create_test_image_with_block((0, 0, 255), (150, 150))
        pos = self.detector.detect(img, 'red', min_area=500, max_area=1000000)
        self.assertIsNotNone(pos, "应检测到红色色块")

    def test_green_block_detection(self):
        """测试绿色色块检测准确率"""
        img = self._create_test_image_with_block((0, 255, 0), (150, 150))
        pos = self.detector.detect(img, 'green', min_area=500, max_area=1000000)
        self.assertIsNotNone(pos, "应检测到绿色色块")

    def test_multiple_colors_no_cross_detect(self):
        """测试不会误检其他颜色"""
        # 纯蓝色图像不应被检测为红色
        img = self._create_test_image_with_block((255, 0, 0), (200, 200))
        pos_red = self.detector.detect(img, 'red', min_area=500, max_area=1000000)
        pos_green = self.detector.detect(img, 'green', min_area=500, max_area=1000000)
        self.assertIsNone(pos_red, "蓝色图像不应被检测为红色")
        self.assertIsNone(pos_green, "蓝色图像不应被检测为绿色")


def run_tests():
    """运行所有测试并返回结果"""
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()

    suite.addTests(loader.loadTestsFromTestCase(TestColorDetector))
    suite.addTests(loader.loadTestsFromTestCase(TestMaterialRecognizer))
    suite.addTests(loader.loadTestsFromTestCase(TestColorAnalysisAccuracy))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    return result


if __name__ == "__main__":
    print("=" * 60)
    print("物料识别系统 - 测试单元")
    print("=" * 60)
    print(f"YOLOv8可用: {'是' if YOLO_AVAILABLE else '否 (pip install ultralytics)'}")
    print()

    result = run_tests()

    print()
    print("=" * 60)
    print(f"测试结果: {result.testsRun} 个测试, "
          f"{len(result.failures)} 失败, "
          f"{len(result.errors)} 错误, "
          f"{len(result.skipped)} 跳过")
    print("=" * 60)

    sys.exit(0 if result.wasSuccessful() else 1)
