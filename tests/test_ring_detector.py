"""
圆环检测测试 - 实时调参版
滑块调节：二值化阈值、圆度、最小/最大半径、高斯模糊
"""
import sys
import os
import cv2
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

WIN_PARAMS = "Params"


def main():
    cap = cv2.VideoCapture(1, cv2.CAP_DSHOW)
    if not cap.isOpened():
        cap = cv2.VideoCapture(1)
    if not cap.isOpened():
        print("无法打开摄像头，程序退出")
        return

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_AUTO_EXPOSURE, 0.25)
    cap.set(cv2.CAP_PROP_EXPOSURE, -10.0)
    cap.set(cv2.CAP_PROP_GAIN, 0)

    # 创建调参窗口和滑块
    cv2.namedWindow(WIN_PARAMS)
    cv2.createTrackbar("Threshold", WIN_PARAMS, 127, 255, lambda x: None)
    cv2.createTrackbar("Circularity", WIN_PARAMS, 70, 100, lambda x: None)
    cv2.createTrackbar("MinRadius", WIN_PARAMS, 30, 200, lambda x: None)
    cv2.createTrackbar("MaxRadius", WIN_PARAMS, 100, 200, lambda x: None)
    cv2.createTrackbar("BlurKernel", WIN_PARAMS, 5, 21, lambda x: None)

    print("操作提示：按 q 关闭程序，拖动滑块实时调参")

    fps = 0
    frame_count = 0
    last_time = cv2.getTickCount()
    morph_kernel = np.ones((3, 3), np.uint8)

    while True:
        ret, frame = cap.read()
        if not ret:
            print("摄像头画面读取失败，退出循环")
            break

        # 读取滑块参数
        thresh_val = cv2.getTrackbarPos("Threshold", WIN_PARAMS)
        circ_raw = cv2.getTrackbarPos("Circularity", WIN_PARAMS)
        min_r = cv2.getTrackbarPos("MinRadius", WIN_PARAMS)
        max_r = cv2.getTrackbarPos("MaxRadius", WIN_PARAMS)
        blur_k = cv2.getTrackbarPos("BlurKernel", WIN_PARAMS)
        if blur_k < 1:
            blur_k = 1
        if blur_k % 2 == 0:
            blur_k += 1  # 必须为奇数
        circularity_thresh = circ_raw / 100.0

        # 确保 min_r < max_r
        if min_r >= max_r:
            max_r = min_r + 1

        # 图像预处理                                                                                                                                                                                                                                                                                                                                                    
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        blur = cv2.GaussianBlur(gray, (blur_k, blur_k), 0)
        _, binary = cv2.threshold(blur, thresh_val, 255, cv2.THRESH_BINARY)
        binary = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, morph_kernel)

        # 轮廓提取
        contours, hierarchy = cv2.findContours(binary, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_SIMPLE)

        display = frame.copy()
        smallest_ring = None  # 只保留最小的圆环

        for idx, cnt in enumerate(contours):
            h_info = hierarchy[0][idx]
            if h_info[3] != -1:
                continue

            area = cv2.contourArea(cnt)
            min_area = np.pi * (min_r ** 2)
            max_area = np.pi * (max_r ** 2)
            if not (min_area < area < max_area):
                continue

            x_rect, y_rect, w, h = cv2.boundingRect(cnt)
            aspect_ratio = w / h
            if not (0.8 < aspect_ratio < 1.2):
                continue

            perimeter = cv2.arcLength(cnt, closed=True)
            if perimeter < 10:
                continue
            circularity = 4 * np.pi * area / (perimeter ** 2)
            if circularity < circularity_thresh:
                continue

            (cx, cy), r = cv2.minEnclosingCircle(cnt)
            center_x, center_y, radius = int(cx), int(cy), int(r)

            # 只保留半径最小的圆环
            if smallest_ring is None or radius < smallest_ring[2]:
                smallest_ring = (center_x, center_y, radius)

        # 只画最小的那个圆环，标出 x,y 坐标
        if smallest_ring:
            sx, sy, sr = smallest_ring
            cv2.circle(display, (sx, sy), sr, (0, 255, 0), 2)
            cv2.circle(display, (sx, sy), 3, (0, 0, 255), -1)
            # 画十字线标出坐标位置
            cv2.line(display, (sx - 15, sy), (sx + 15, sy), (0, 0, 255), 1)
            cv2.line(display, (sx, sy - 15), (sx, sy + 15), (0, 0, 255), 1)
            # 显示坐标
            coord_text = f"X:{sx} Y:{sy} R:{sr}px"
            cv2.putText(display, coord_text, (sx + sr + 8, sy - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 255), 2)

        # 帧率计算
        frame_count += 1
        current_time = cv2.getTickCount()
        elapsed = (current_time - last_time) / cv2.getTickFrequency()
        if elapsed >= 1.0:
            fps = frame_count / elapsed
            frame_count = 0
            last_time = current_time

        cv2.putText(display, f"Smallest Ring  FPS: {fps:.1f}", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)

        # 在画面上显示当前参数
        param_text = (f"Thresh:{thresh_val} Circ:{circularity_thresh:.2f} "
                      f"R:{min_r}-{max_r} Blur:{blur_k}")
        cv2.putText(display, param_text, (10, 460),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.45, (200, 200, 200), 1)

        cv2.imshow("Original_Detect", display)
        cv2.imshow("Binary_Img", binary)

        key = cv2.waitKey(30) & 0xFF
        if key == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
