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
    qr_fps_counter = FPSCounter(update_interval=10)
    last_qr_data = None
    qr_display_time = 0

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

                # 显示二维码摄像头画面并扫码
                qr_success, qr_img = vision.camera.read_qr()
                if qr_success and qr_img is not None:
                    qr_fps = qr_fps_counter.tick()
                    cv2.putText(qr_img, f"QR FPS: {qr_fps:.1f}",
                                (10, 30), cv2.FONT_HERSHEY_SIMPLEX,
                                0.8, (0, 255, 0), 2)

                    # 识别二维码
                    qr_data = vision.qr_detector.detect(qr_img)
                    if qr_data:
                        last_qr_data = qr_data
                        qr_display_time = time.time()

                        # 解析任务码
                        task_result = vision.task_parser.parse(qr_data)
                        if task_result:
                            batch1_colors, batch1_pos, batch2_colors, batch2_pos = task_result
                            color_names = ['', '红', '蓝', '绿', '黄', '黑', '浅蓝']

                            # 显示任务码
                            y_offset = 60
                            cv2.putText(qr_img, f"Task: {qr_data}", (10, y_offset),
                                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)

                            y_offset += 30
                            cv2.putText(qr_img, "Batch1:", (10, y_offset),
                                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
                            for i, (c, p) in enumerate(zip(batch1_colors, batch1_pos)):
                                name = color_names[c] if c < len(color_names) else str(c)
                                cv2.putText(qr_img, f"  {name}#{p}", (10, y_offset + 25 + i * 20),
                                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)

                            y_offset += 90
                            cv2.putText(qr_img, "Batch2:", (10, y_offset),
                                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
                            for i, (c, p) in enumerate(zip(batch2_colors, batch2_pos)):
                                name = color_names[c] if c < len(color_names) else str(c)
                                cv2.putText(qr_img, f"  {name}#{p}", (10, y_offset + 25 + i * 20),
                                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
                        else:
                            cv2.putText(qr_img, f"QR: {qr_data}", (10, 60),
                                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
                            cv2.putText(qr_img, "Invalid task code", (10, 90),
                                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)
                    elif last_qr_data and (time.time() - qr_display_time) < 3:
                        # 识别失败但3秒内显示上次结果
                        cv2.putText(qr_img, f"Last: {last_qr_data}", (10, 60),
                                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
                    else:
                        cv2.putText(qr_img, "No QR detected", (10, 60),
                                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)

                    cv2.imshow('QR Camera', qr_img)

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