"""数字识别测试 - ANN_MLP 神经网络版（自适应阈值 + 闭运算 + 轮廓留白）"""
import sys
import os
import cv2
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

WIN_PARAMS = "Params"
IMG_SIZE = 32  # 加大到 32x32，保留更多细节
TARGET_DIGITS = [1, 2, 3]
MODEL_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'digit_model.xml')
PAD = 3  # 轮廓边缘留白，防止裁剪丢失笔画


def load_images_from_folder(data_dir):
    """从 digit_data/ 加载图片，多尺度增强"""
    all_imgs = []
    all_labels = []

    for digit in TARGET_DIGITS:
        digit_dir = os.path.join(data_dir, str(digit))
        if not os.path.isdir(digit_dir):
            print(f"  警告: 找不到 {digit_dir}")
            continue
        for fname in os.listdir(digit_dir):
            if not fname.lower().endswith(('.png', '.jpg', '.bmp')):
                continue
            img = cv2.imread(os.path.join(digit_dir, fname), 0)
            if img is None:
                continue
            if np.mean(img) < 127:
                img = 255 - img
            # 多尺度增强：模拟远处小数字
            for scale in [1.0, 0.7, 0.5, 0.35]:
                if scale < 1.0:
                    small = cv2.resize(img, None, fx=scale, fy=scale)
                    img_scaled = cv2.resize(small, (IMG_SIZE, IMG_SIZE), interpolation=cv2.INTER_CUBIC)
                else:
                    img_scaled = cv2.resize(img, (IMG_SIZE, IMG_SIZE))
                all_imgs.append(img_scaled.flatten().astype(np.float32) / 255.0)
                all_labels.append(digit)

    return np.array(all_imgs), np.array(all_labels)


def generate_synthetic_data(samples_per_digit=200):
    """生成合成数据：多字体多尺寸"""
    fonts = [
        cv2.FONT_HERSHEY_SIMPLEX,
        cv2.FONT_HERSHEY_DUPLEX,
        cv2.FONT_HERSHEY_COMPLEX,
        cv2.FONT_HERSHEY_TRIPLEX,
        cv2.FONT_HERSHEY_PLAIN,
    ]
    all_imgs = []
    all_labels = []

    for digit in TARGET_DIGITS:
        count = 0
        for font in fonts:
            for scale10 in range(4, 14):
                for thick in range(1, 4):
                    if count >= samples_per_digit:
                        break
                    scale = scale10 / 10.0
                    img = np.zeros((IMG_SIZE, IMG_SIZE), dtype=np.uint8)
                    text = str(digit)
                    (tw, th), _ = cv2.getTextSize(text, font, scale, thick)
                    x = (IMG_SIZE - tw) // 2
                    y = (IMG_SIZE + th) // 2
                    cv2.putText(img, text, (x, y), font, scale, 255, thick, cv2.LINE_AA)

                    tx = np.random.randint(-2, 3)
                    ty = np.random.randint(-2, 3)
                    M = np.float32([[1, 0, tx], [0, 1, ty]])
                    img = cv2.warpAffine(img, M, (IMG_SIZE, IMG_SIZE))

                    all_imgs.append(img.flatten().astype(np.float32) / 255.0)
                    all_labels.append(digit)
                    count += 1
                if count >= samples_per_digit:
                    break
            if count >= samples_per_digit:
                break
        print(f"  数字 {digit}: 生成 {count} 张")

    return np.array(all_imgs), np.array(all_labels)


def train_neural_network():
    """训练 ANN_MLP，缓存到磁盘"""
    if os.path.exists(MODEL_PATH):
        print(f"加载已保存的模型: {MODEL_PATH}")
        ann = cv2.ml.ANN_MLP_load(MODEL_PATH)
        print("模型加载成功，跳过训练")
        return ann

    data_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'digit_data')

    if os.path.isdir(data_dir):
        print("从 digit_data/ 加载图片...")
        all_imgs, all_labels = load_images_from_folder(data_dir)
        if len(all_imgs) == 0:
            print("  没加载到图片，使用合成数据")
            all_imgs, all_labels = generate_synthetic_data()
        else:
            for d in TARGET_DIGITS:
                cnt = np.sum(all_labels == d)
                print(f"  数字 {d}: {cnt} 张")
    else:
        print("使用合成数据...")
        all_imgs, all_labels = generate_synthetic_data()

    # 打乱并划分 80% 训练 / 20% 测试
    indices = np.random.permutation(len(all_labels))
    split = int(len(all_labels) * 0.8)
    train_idx, test_idx = indices[:split], indices[split:]

    train_data = all_imgs[train_idx]
    train_labels = all_labels[train_idx]
    test_data = all_imgs[test_idx]
    test_labels = all_labels[test_idx]

    print(f"训练集: {len(train_data)}, 测试集: {len(test_data)}")

    # 标签映射: 1→0, 2→1, 3→2
    label_map = {d: i for i, d in enumerate(TARGET_DIGITS)}
    train_labels_mapped = np.array([label_map[l] for l in train_labels])

    # one-hot 编码（3 类）
    train_labels_oh = np.zeros((len(train_labels), 3), dtype=np.float32)
    for i, l in enumerate(train_labels_mapped):
        train_labels_oh[i, l] = 1.0

    # 网络结构：32x32=1024 输入
    ann = cv2.ml.ANN_MLP_create()
    ann.setLayerSizes(np.array([1024, 256, 128, 3]))
    ann.setActivationFunction(cv2.ml.ANN_MLP_SIGMOID_SYM, 2.0, 1.0)
    ann.setTrainMethod(cv2.ml.ANN_MLP_BACKPROP, 0.05, 0.05)
    ann.setTermCriteria((cv2.TERM_CRITERIA_MAX_ITER | cv2.TERM_CRITERIA_EPS, 2000, 0.0001))

    print("正在训练...")
    ann.train(train_data, cv2.ml.ROW_SAMPLE, train_labels_oh)

    # 测试
    ret, result = ann.predict(test_data)
    predicted_mapped = np.argmax(result, axis=1)
    reverse_map = {i: d for d, i in label_map.items()}
    predicted = np.array([reverse_map[p] for p in predicted_mapped])
    accuracy = np.mean(predicted == test_labels) * 100
    print(f"训练完成，测试准确率: {accuracy:.1f}%")

    for d in TARGET_DIGITS:
        mask = test_labels == d
        if np.sum(mask) > 0:
            acc = np.mean(predicted[mask] == d) * 100
            print(f"  数字 {d}: {acc:.0f}% ({np.sum(mask)}张)")

    ann.save(MODEL_PATH)
    print(f"模型已保存: {MODEL_PATH}")

    return ann


def robust_preprocess(img_bgr):
    """
    抗干扰预处理流水线（借鉴 TensorRT 版）
    - 自适应阈值：抗反光、远距离明暗不均
    - 闭运算：修复远距离断裂笔画
    """
    gray = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY)
    blur = cv2.GaussianBlur(gray, (3, 3), 0)
    # 自适应阈值，抗反光和明暗不均
    bin_img = cv2.adaptiveThreshold(
        blur, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, cv2.THRESH_BINARY_INV, 15, 3
    )
    # 闭运算修复远距离断裂笔画
    kernel = np.ones((2, 2), np.uint8)
    bin_img = cv2.morphologyEx(bin_img, cv2.MORPH_CLOSE, kernel)
    return bin_img


def extract_digit_roi(bin_img, min_w, max_w, min_h, max_h):
    """
    提取数字 ROI，带边缘留白（借鉴 TensorRT 版）
    返回: [(roi_img, x1, y1, x2, y2), ...]
    """
    contours, _ = cv2.findContours(bin_img, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    roi_list = []
    h_img, w_img = bin_img.shape

    for cnt in contours:
        x, y, w, h = cv2.boundingRect(cnt)
        if not (min_w < w < max_w and min_h < h < max_h):
            continue
        # 宽高比过滤
        aspect_ratio = float(w) / h
        if not (0.3 <= aspect_ratio <= 0.8):
            continue
        # 边缘留白，防止裁剪丢失笔画
        x1 = max(0, x - PAD)
        y1 = max(0, y - PAD)
        x2 = min(w_img, x + w + PAD)
        y2 = min(h_img, y + h + PAD)
        roi = bin_img[y1:y2, x1:x2]
        roi_list.append((roi, x1, y1, x2, y2))

    return roi_list


def recognize_digit(ann, roi):
    """用神经网络识别数字，返回 1/2/3"""
    # roi 已经是二值图（白字黑底），直接 resize
    roi_resized = cv2.resize(roi, (IMG_SIZE, IMG_SIZE))
    # 确保白字黑底
    if np.mean(roi_resized) < 127:
        roi_resized = 255 - roi_resized
    roi_float = roi_resized.flatten().astype(np.float32).reshape(1, -1) / 255.0

    ret, result = ann.predict(roi_float)
    idx = int(np.argmax(result, axis=1)[0])
    reverse_map = {0: TARGET_DIGITS[0], 1: TARGET_DIGITS[1], 2: TARGET_DIGITS[2]}
    return reverse_map.get(idx, -1)


def main():
    import time

    # 先训练模型（不需要摄像头）
    ann = train_neural_network()

    # 打开摄像头，带重试
    cap = None
    for attempt in range(3):
        cap = cv2.VideoCapture(1, cv2.CAP_DSHOW)
        if cap.isOpened():
            break
        print(f"摄像头打开失败，重试 {attempt + 1}/3...")
        cap.release()
        time.sleep(1)

    if cap is None or not cap.isOpened():
        cap = cv2.VideoCapture(1)
        if not cap.isOpened():
            print("无法打开摄像头，程序退出")
            return

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_AUTO_EXPOSURE, 0.25)
    cap.set(cv2.CAP_PROP_EXPOSURE, -5)
    cap.set(cv2.CAP_PROP_GAIN, 0)

    time.sleep(0.5)
    for _ in range(5):
        cap.read()

    cv2.namedWindow(WIN_PARAMS)
    cv2.createTrackbar("MinW", WIN_PARAMS, 10, 80, lambda x: None)
    cv2.createTrackbar("MaxW", WIN_PARAMS, 60, 120, lambda x: None)
    cv2.createTrackbar("MinH", WIN_PARAMS, 15, 90, lambda x: None)
    cv2.createTrackbar("MaxH", WIN_PARAMS, 70, 130, lambda x: None)
    # 自适应阈值的 C 参数（值越大越严格）
    cv2.createTrackbar("ThreshC", WIN_PARAMS, 3, 20, lambda x: None)

    print("操作提示：按 q 关闭程序，拖动滑块实时调参")

    fps = 0
    frame_count = 0
    last_time = cv2.getTickCount()

    while True:
        ret, frame = cap.read()
        if not ret:
            print("摄像头画面读取失败，退出循环")
            break

        min_w = cv2.getTrackbarPos("MinW", WIN_PARAMS)
        max_w = cv2.getTrackbarPos("MaxW", WIN_PARAMS)
        min_h = cv2.getTrackbarPos("MinH", WIN_PARAMS)
        max_h = cv2.getTrackbarPos("MaxH", WIN_PARAMS)
        thresh_c = cv2.getTrackbarPos("ThreshC", WIN_PARAMS)

        if min_w >= max_w:
            max_w = min_w + 1
        if min_h >= max_h:
            max_h = min_h + 1

        # 抗干扰预处理
        binary = robust_preprocess(frame)

        # 提取 ROI（带留白）
        roi_list = extract_digit_roi(binary, min_w, max_w, min_h, max_h)

        display = frame.copy()
        for roi, x1, y1, x2, y2 in roi_list:
            cv2.rectangle(display, (x1, y1), (x2, y2), (0, 255, 0), 2)
            digit = recognize_digit(ann, roi)
            cv2.putText(display, str(digit), (x1, y1 - 6),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

        # 帧率
        frame_count += 1
        current_time = cv2.getTickCount()
        elapsed = (current_time - last_time) / cv2.getTickFrequency()
        if elapsed >= 1.0:
            fps = frame_count / elapsed
            frame_count = 0
            last_time = current_time

        cv2.putText(display, f"Digits: {len(roi_list)}  FPS: {fps:.1f}", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)

        cv2.imshow("Digit Test", display)
        cv2.imshow("Binary", binary)

        key = cv2.waitKey(30) & 0xFF
        if key == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
