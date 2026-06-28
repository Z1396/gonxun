"""数字采集工具 - 对着数字按 1/2/3 自动保存到对应文件夹"""
import sys
import os
import cv2
import numpy as np
import time

DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'digit_data')

def main():
    # 确保文件夹存在
    for d in [1, 2, 3]:
        os.makedirs(os.path.join(DATA_DIR, str(d)), exist_ok=True)

    cap = cv2.VideoCapture(1, cv2.CAP_DSHOW)
    if not cap.isOpened():
        cap = cv2.VideoCapture(1)
    if not cap.isOpened():
        print("无法打开摄像头")
        return

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_AUTO_EXPOSURE, 0.25)
    cap.set(cv2.CAP_PROP_EXPOSURE, -5)

    time.sleep(0.5)
    for _ in range(5):
        cap.read()

    print("=" * 50)
    print("数字采集工具")
    print("=" * 50)
    print("操作说明：")
    print("  按 1/2/3  - 保存当前画面到对应数字文件夹")
    print("  按 s      - 选择框选区域保存（更精准）")
    print("  按 q      - 退出")
    print("=" * 50)

    # 统计已采集数量
    counts = {}
    for d in [1, 2, 3]:
        folder = os.path.join(DATA_DIR, str(d))
        counts[d] = len([f for f in os.listdir(folder) if f.endswith('.png')])
    print(f"已采集: 1={counts[1]}张, 2={counts[2]}张, 3={counts[3]}张")
    print("建议每个数字采集 50~100 张")
    print("=" * 50)

    selecting = False
    roi_box = None
    roi_start = None

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        display = frame.copy()

        # 显示统计
        for d in [1, 2, 3]:
            folder = os.path.join(DATA_DIR, str(d))
            cnt = len([f for f in os.listdir(folder) if f.endswith('.png')])
            cv2.putText(display, f"{d}: {cnt}", (10 + (d-1)*100, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

        # 框选模式
        if selecting and roi_start is not None:
            cv2.rectangle(display, roi_start, (roi_box[2], roi_box[3]), (0, 255, 255), 2)
            cv2.putText(display, "框选区域，按 1/2/3 保存", (10, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)

        cv2.imshow("Collect Digits", display)

        key = cv2.waitKey(30) & 0xFF

        if key == ord('q'):
            break
        elif key in [ord('1'), ord('2'), ord('3')]:
            digit = int(chr(key))
            if selecting and roi_box is not None:
                # 保存框选区域
                x1, y1, x2, y2 = roi_box
                roi = frame[y1:y2, x1:x2]
            else:
                # 保存全画面
                roi = frame

            # 转灰度 + 二值化
            gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)
            _, binary = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
            # 白字黑底
            if np.mean(binary) < 127:
                binary = 255 - binary

            # 保存
            folder = os.path.join(DATA_DIR, str(digit))
            timestamp = int(time.time() * 1000)
            filename = os.path.join(folder, f"{timestamp}.png")
            cv2.imwrite(filename, binary)
            print(f"保存: {filename}")

            # 更新统计
            counts[digit] += 1

            # 重置框选
            selecting = False
            roi_box = None
            roi_start = None

        elif key == ord('s'):
            # 进入框选模式
            selecting = True
            print("框选模式：用鼠标拖动框选数字区域")

        # 鼠标框选回调
        def mouse_callback(event, x, y, flags, param):
            global roi_start, roi_box
            if event == cv2.EVENT_LBUTTONDOWN:
                roi_start = (x, y)
            elif event == cv2.EVENT_MOUSEMOVE and roi_start is not None:
                roi_box = [roi_start[0], roi_start[1], x, y]
            elif event == cv2.EVENT_LBUTTONUP:
                roi_box = [roi_start[0], roi_start[1], x, y]

        cv2.setMouseCallback("Collect Digits", mouse_callback)

    cap.release()
    cv2.destroyAllWindows()

    # 最终统计
    print("=" * 50)
    print("采集完成！")
    for d in [1, 2, 3]:
        folder = os.path.join(DATA_DIR, str(d))
        cnt = len([f for f in os.listdir(folder) if f.endswith('.png')])
        print(f"  数字 {d}: {cnt} 张")
    print("运行 python tests/test_digit_cnn.py 重新训练")


if __name__ == '__main__':
    main()