#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
工创赛2025智能物流搬运 - 视觉系统入口

使用方式:
  python vision_system.py            # 启动视觉主循环
  python vision_system.py test       # 运行单元测试
"""
import cv2
import time
import logging
import config

logging.basicConfig(level=getattr(logging, config.LOG_LEVEL), format=config.LOG_FORMAT)

from vision import VisionSystem, FPSCounter, check_gui_available, generate_test_frame


def main():
    """程序入口：实例化视觉系统并运行主循环"""
    print("=" * 50)
    print("工创赛2025智能物流视觉系统 v3.0 (模块化)")
    print("=" * 50)

    vision = VisionSystem(serial_mock=True)
    vision.serial_comm.start()
    vision.camera.open()

    print("\n系统已启动，按 'q' 退出")
    print("模式说明: 0=待机, 1=三色物料, 3=色环定位, 4=码垛定位, 9=二维码")

    gui_available = check_gui_available()
    if not gui_available:
        print("[注意] headless模式，不支持cv2.imshow图形窗口")

    fps_counter = FPSCounter(update_interval=10)

    try:
        while True:
            success, img = vision.camera.read_main()
            if not success:
                img = generate_test_frame()

            processed_img = vision.process_frame(img)
            fps = fps_counter.tick()

            if gui_available:
                cv2.putText(processed_img, f"FPS: {fps:.1f}",
                            (processed_img.shape[1] - 120, 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
                cv2.imshow('Vision System', processed_img)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            else:
                print(f"[帧] 模式={vision.serial_comm.unit} FPS={fps:.1f}")
                time.sleep(0.5)

    except KeyboardInterrupt:
        print("\n用户中断")
    finally:
        vision.camera.close()
        if gui_available:
            cv2.destroyAllWindows()
        print("系统已关闭")


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == 'test':
        from tests.test_vision import run_all_tests
        success = run_all_tests()
        sys.exit(0 if success else 1)
    else:
        main()