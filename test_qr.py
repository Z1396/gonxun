#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
二维码识别测试
测试指定图片的二维码识别和任务码解析
"""
import cv2
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from vision import QRDetector, TaskCodeParser


def test_qr_image(img_path):
    """测试单张二维码图片"""
    print(f"测试图片: {img_path}")
    
    img = cv2.imread(img_path)
    if img is None:
        print(f"无法读取图片: {img_path}")
        return
    
    # 识别二维码
    qr = QRDetector()
    data = qr.detect(img)
    
    if not data:
        print("未检测到二维码")
        return
    
    print(f"二维码内容: {data}")
    
    # 解析任务码
    parser = TaskCodeParser()
    result = parser.parse(data)
    
    if result:
        batch1_colors, batch1_pos, batch2_colors, batch2_pos = result
        color_names = ['', '红', '蓝', '绿', '黄', '黑', '浅蓝']
        
        print("\n任务码解析成功:")
        print(f"  Batch1: ", end="")
        for c, p in zip(batch1_colors, batch1_pos):
            name = color_names[c] if c < len(color_names) else str(c)
            print(f"{name}#{p} ", end="")
        print()
        
        print(f"  Batch2: ", end="")
        for c, p in zip(batch2_colors, batch2_pos):
            name = color_names[c] if c < len(color_names) else str(c)
            print(f"{name}#{p} ", end="")
        print()
        
        # 绘制结果（手动绘制，避免detect_and_draw的浮点问题）
        annotated = img.copy()
        
        # 添加文字
        y = 30
        cv2.putText(annotated, f"Task: {data}", (10, y),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)
        
        y += 30
        cv2.putText(annotated, "Batch1:", (10, y),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
        for i, (c, p) in enumerate(zip(batch1_colors, batch1_pos)):
            name = color_names[c] if c < len(color_names) else str(c)
            cv2.putText(annotated, f"  {name}#{p}", (10, y + 25 + i * 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
        
        y += 90
        cv2.putText(annotated, "Batch2:", (10, y),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
        for i, (c, p) in enumerate(zip(batch2_colors, batch2_pos)):
            name = color_names[c] if c < len(color_names) else str(c)
            cv2.putText(annotated, f"  {name}#{p}", (10, y + 25 + i * 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
        
        # 保存结果
        out_path = os.path.splitext(img_path)[0] + "_result.png"
        cv2.imwrite(out_path, annotated)
        print(f"\n结果已保存: {out_path}")
        
        # 显示
        cv2.imshow('QR Result', annotated)
        cv2.waitKey(0)
        cv2.destroyAllWindows()
    else:
        print("任务码解析失败")


if __name__ == "__main__":
    if len(sys.argv) > 1:
        test_qr_image(sys.argv[1])
    else:
        test_qr_image("123+231+231+456.png")
