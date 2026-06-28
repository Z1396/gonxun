"""物料图片采集工具 - 拍照保存到 raw_images/"""
import os
import cv2
import time

RAW_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'yolo_pipeline', 'raw_images')
os.makedirs(RAW_DIR, exist_ok=True)

def main():
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

    count = len([f for f in os.listdir(RAW_DIR) if f.lower().endswith(('.jpg', '.png'))])

    print("=" * 50)
    print("物料图片采集工具")
    print("=" * 50)
    print(f"保存目录: {RAW_DIR}")
    print(f"已采集: {count} 张")
    print("操作: 按 空格 拍照, 按 q 退出")
    print("建议: 每个物料拍 50~100 张，不同角度/距离/光照")
    print("=" * 50)

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        cv2.putText(frame, f"Count: {count}", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        cv2.putText(frame, "SPACE=拍照  Q=退出", (10, 60),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)

        cv2.imshow("Collect Materials", frame)

        key = cv2.waitKey(30) & 0xFF
        if key == ord('q'):
            break
        elif key == ord(' '):
            timestamp = int(time.time() * 1000)
            filename = os.path.join(RAW_DIR, f"material_{timestamp}.jpg")
            cv2.imwrite(filename, frame)
            count += 1
            print(f"已保存: {filename} (共 {count} 张)")

    cap.release()
    cv2.destroyAllWindows()
    print(f"\n采集完成！共 {count} 张，保存在: {RAW_DIR}")
    print("下一步: 用 LabelMe 标注，然后运行 python run_pipeline.py --step prepare,train")


if __name__ == '__main__':
    main()