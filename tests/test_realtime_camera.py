#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
实时摄像头识别测试程序
使用C100摄像头实时测试物料、圆环、二维码、障碍物识别效果
按数字键切换测试模式：
  1 - 物料色块检测 (红/绿/蓝)
  2 - 三环定位检测
  3 - 六环评分检测
  4 - 二维码识别
  5 - 黑色障碍物检测
  q - 退出程序
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
    CameraManager
)


class RealtimeCameraTest:
    """实时摄像头测试"""

    def __init__(self, main_camera=1):
        self.main_camera = main_camera
        self.camera = CameraManager(main_index=main_camera, qr_index=2)
        self.detectors = {
            'color': ColorDetector(),
            'three_ring': ThreeRingDetector(),
            'six_ring': SixRingDetector(),
            'qr': QRDetector(),
            'obstacle': ObstacleDetector()
        }
        self.current_mode = 'color'

    def draw_text_with_background(self, img, text, pos, font=cv2.FONT_HERSHEY_SIMPLEX,
                                  font_scale=0.7, text_color=(255, 255, 255),
                                  bg_color=(0, 100, 0), thickness=2):
        """绘制带背景的文字"""
        text_size, _ = cv2.getTextSize(text, font, font_scale, thickness)
        text_w, text_h = text_size
        x, y = pos
        cv2.rectangle(img, (x - 5, y - text_h - 5), (x + text_w + 5, y + 5), bg_color, -1)
        cv2.putText(img, text, (x, y), font, font_scale, text_color, thickness)

    def process_color_mode(self, img):
        """物料色块检测模式"""
        result = img.copy()

        # 检测红、绿、蓝三种颜色
        red_pos = self.detectors['color'].detect(img, 'red', 2000)
        green_pos = self.detectors['color'].detect(img, 'green', 2000)
        blue_pos = self.detectors['color'].detect(img, 'blue', 2000)

        # 绘制检测结果
        if red_pos:
            cv2.circle(result, red_pos, 10, (0, 0, 255), -1)
            cv2.putText(result, f"R:({red_pos[0]},{red_pos[1]})", (red_pos[0] - 40, red_pos[1] + 30),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)

        if green_pos:
            cv2.circle(result, green_pos, 10, (0, 255, 0), -1)
            cv2.putText(result, f"G:({green_pos[0]},{green_pos[1]})", (green_pos[0] - 40, green_pos[1] + 30),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)

        if blue_pos:
            cv2.circle(result, blue_pos, 10, (255, 0, 0), -1)
            cv2.putText(result, f"B:({blue_pos[0]},{blue_pos[1]})", (blue_pos[0] - 40, blue_pos[1] + 30),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 0), 2)

        # 绘制信息栏
        self.draw_text_with_background(result, "模式1: 物料色块检测", (10, 30), bg_color=(50, 50, 150))
        status = f"检测到: R={'V' if red_pos else 'X'} G={'V' if green_pos else 'X'} B={'V' if blue_pos else 'X'}"
        self.draw_text_with_background(result, status, (10, 60), bg_color=(80, 80, 80))

        return result

    def process_three_ring_mode(self, img):
        """三环定位检测模式"""
        result = img.copy()

        ring_pos = self.detectors['three_ring'].detect(result)

        if ring_pos:
            # ring_pos 是 (x1,y1,x2,y2,x3,y3)
            positions = [(ring_pos[0], ring_pos[1]), (ring_pos[2], ring_pos[3]), (ring_pos[4], ring_pos[5])]
            labels = ["L", "M", "R"]
            for i, (pos, label) in enumerate(zip(positions, labels)):
                cv2.circle(result, pos, 10, (0, 255, 255), -1)
                cv2.putText(result, f"{label}:({pos[0]},{pos[1]})", (pos[0] - 30, pos[1] - 20),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)

            self.draw_text_with_background(result, "模式2: 三环定位检测", (10, 30), bg_color=(0, 150, 150))
            self.draw_text_with_background(result, f"检测到3个环", (10, 60), bg_color=(80, 80, 80))
        else:
            self.draw_text_with_background(result, "模式2: 三环定位检测", (10, 30), bg_color=(0, 150, 150))
            self.draw_text_with_background(result, "未检测到3个环", (10, 60), bg_color=(50, 50, 50))

        return result

    def process_six_ring_mode(self, img):
        """六环评分检测模式"""
        result = img.copy()

        rings = self.detectors['six_ring'].detect(img)

        if rings:
            for i, (pos, radius) in enumerate(rings.items()):
                cv2.circle(result, pos, radius, (0, 0, 255), 2)
                cv2.putText(result, f"{i+1}", (pos[0] - 5, pos[1] + 5),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)

            self.draw_text_with_background(result, "模式3: 六环评分检测", (10, 30), bg_color=(150, 0, 150))
            self.draw_text_with_background(result, f"检测到{len(rings)}个环", (10, 60), bg_color=(80, 80, 80))
        else:
            self.draw_text_with_background(result, "模式3: 六环评分检测", (10, 30), bg_color=(150, 0, 150))
            self.draw_text_with_background(result, "未检测到环", (10, 60), bg_color=(50, 50, 50))

        return result

    def process_qr_mode(self, img):
        """二维码识别模式"""
        result = img.copy()

        # 尝试主摄像头
        qr_data = self.detectors['qr'].detect(img)

        # 如果主摄像头没识别到，尝试二维码专用相机
        if not qr_data:
            success, qr_img = self.camera.read_qr()
            if success and qr_img is not None:
                qr_data = self.detectors['qr'].detect(qr_img)

        if qr_data:
            self.draw_text_with_background(result, "模式4: 二维码识别", (10, 30), bg_color=(0, 150, 0))
            self.draw_text_with_background(result, f"内容: {qr_data}", (10, 60), bg_color=(50, 100, 50))
        else:
            self.draw_text_with_background(result, "模式4: 二维码识别", (10, 30), bg_color=(0, 150, 0))
            self.draw_text_with_background(result, "未检测到二维码", (10, 60), bg_color=(50, 50, 50))

        return result

    def process_obstacle_mode(self, img):
        """黑色障碍物检测模式"""
        result = img.copy()

        obstacles = self.detectors['obstacle'].detect(img)

        if obstacles:
            for cx, cy, r in obstacles:
                cv2.circle(result, (cx, cy), r, (0, 0, 255), 2)
                cv2.circle(result, (cx, cy), 3, (0, 255, 0), -1)
                cv2.putText(result, f"({cx},{cy})", (cx - 25, cy - 15),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)

            self.draw_text_with_background(result, "模式5: 障碍物检测", (10, 30), bg_color=(100, 0, 0))
            self.draw_text_with_background(result, f"检测到{len(obstacles)}个障碍物", (10, 60), bg_color=(80, 80, 80))
        else:
            self.draw_text_with_background(result, "模式5: 障碍物检测", (10, 30), bg_color=(100, 0, 0))
            self.draw_text_with_background(result, "未检测到障碍物", (10, 60), bg_color=(50, 50, 50))

        return result

    def process_frame(self, img):
        """根据当前模式处理帧"""
        if self.current_mode == 'color':
            return self.process_color_mode(img)
        elif self.current_mode == 'three_ring':
            return self.process_three_ring_mode(img)
        elif self.current_mode == 'six_ring':
            return self.process_six_ring_mode(img)
        elif self.current_mode == 'qr':
            return self.process_qr_mode(img)
        elif self.current_mode == 'obstacle':
            return self.process_obstacle_mode(img)
        return img

    def run(self):
        """运行主循环"""
        print("=" * 60)
        print("实时摄像头识别测试")
        print("=" * 60)
        print("按 1-5 切换模式, q 退出")
        print("1: 物料色块 | 2: 三环定位 | 3: 六环评分 | 4: 二维码 | 5: 障碍物")
        print("=" * 60)

        # 打开摄像头
        self.camera.open()

        try:
            while True:
                success, img = self.camera.read_main()
                if not success or img is None:
                    continue

                # 处理并显示
                result = self.process_frame(img)

                # 显示模式切换提示
                mode_tips = {
                    'color': '1:物料 2:三环 3:六环 4:二维码 5:障碍物 | 当前: 物料色块',
                    'three_ring': '1:物料 2:三环 3:六环 4:二维码 5:障碍物 | 当前: 三环定位',
                    'six_ring': '1:物料 2:三环 3:六环 4:二维码 5:障碍物 | 当前: 六环评分',
                    'qr': '1:物料 2:三环 3:六环 4:二维码 5:障碍物 | 当前: 二维码',
                    'obstacle': '1:物料 2:三环 3:六环 4:二维码 5:障碍物 | 当前: 障碍物'
                }
                cv2.putText(result, mode_tips[self.current_mode], (10, result.shape[0] - 15),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)

                cv2.imshow("Camera Test", result)

                # 按键处理
                key = cv2.waitKey(1) & 0xFF
                if key == ord('q'):
                    break
                elif key == ord('1'):
                    self.current_mode = 'color'
                    print("切换到: 物料色块检测")
                elif key == ord('2'):
                    self.current_mode = 'three_ring'
                    print("切换到: 三环定位检测")
                elif key == ord('3'):
                    self.current_mode = 'six_ring'
                    print("切换到: 六环评分检测")
                elif key == ord('4'):
                    self.current_mode = 'qr'
                    print("切换到: 二维码识别")
                elif key == ord('5'):
                    self.current_mode = 'obstacle'
                    print("切换到: 障碍物检测")

        finally:
            self.camera.close()
            cv2.destroyAllWindows()
            print("已退出测试")


def main():
    test = RealtimeCameraTest(main_camera=1)
    test.run()


if __name__ == "__main__":
    main()
