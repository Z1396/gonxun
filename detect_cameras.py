#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
摄像头检测工具
功能：扫描系统中所有可用摄像头，显示其索引和基本信息
使用：python detect_cameras.py
"""
import cv2


def detect_cameras(max_index=10):
    """检测所有可用摄像头"""
    print("正在检测摄像头...")
    print("=" * 50)
    
    available = []
    
    for i in range(max_index):
        cap = cv2.VideoCapture(i)
        if cap.isOpened():
            ret, frame = cap.read()
            if ret:
                width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
                height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
                backend = cap.getBackendName()
                
                info = {
                    'index': i,
                    'width': width,
                    'height': height,
                    'backend': backend
                }
                available.append(info)
                
                print(f"\n✅ 摄像头 [{i}]")
                print(f"   分辨率: {width}x{height}")
                print(f"   后端: {backend}")
                
                # 显示一帧预览
                cv2.imshow(f'Camera {i}', frame)
                cv2.waitKey(100)
            cap.release()
    
    print("\n" + "=" * 50)
    print(f"共找到 {len(available)} 个摄像头")
    print("=" * 50)
    
    if available:
        print("\n推荐配置:")
        if len(available) >= 2:
            print(f"  主摄像头(物料识别): index={available[0]['index']}")
            print(f"  扫码摄像头(二维码): index={available[1]['index']}")
        else:
            print(f"  主摄像头(物料识别): index={available[0]['index']}")
            print(f"  扫码摄像头: 未检测到第二个摄像头")
    
    print("\n按任意键关闭窗口...")
    cv2.waitKey(0)
    cv2.destroyAllWindows()
    
    return available


if __name__ == "__main__":
    cameras = detect_cameras()
