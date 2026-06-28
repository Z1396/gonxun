"""数字识别测试 - TensorFlow CNN 版（卷积神经网络，抗模糊/噪点/反光）"""
import sys
import os
import cv2
import numpy as np

# TensorFlow/Keras
import tensorflow as tf
from tensorflow.keras.models import Sequential, load_model
from tensorflow.keras.layers import Conv2D, MaxPooling2D, Flatten, Dense, Dropout
from tensorflow.keras.utils import to_categorical

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

WIN_PARAMS = "Params"
IMG_SIZE = 32
TARGET_DIGITS = [1, 2, 3]
MODEL_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'digit_cnn.h5')
PAD = 3


def load_images_from_folder(data_dir):
    """从 digit_data/ 加载图片，多尺度增强"""
    all_imgs = []
    all_labels = []

    for digit in TARGET_DIGITS:
        digit_dir = os.path.join(data_dir, str(digit))
        if not os.path.isdir(digit_dir):
            continue
        for fname in os.listdir(digit_dir):
            if not fname.lower().endswith(('.png', '.jpg', '.bmp')):
                continue
            img = cv2.imread(os.path.join(digit_dir, fname), 0)
            if img is None:
                continue
            if np.mean(img) < 127:
                img = 255 - img
            # 多尺度增强
            for scale in [1.0, 0.7, 0.5, 0.35]:
                if scale < 1.0:
                    small = cv2.resize(img, None, fx=scale, fy=scale)
                    img_scaled = cv2.resize(small, (IMG_SIZE, IMG_SIZE), interpolation=cv2.INTER_CUBIC)
                else:
                    img_scaled = cv2.resize(img, (IMG_SIZE, IMG_SIZE))
                # 加噪声增强鲁棒性
                noise = np.random.normal(0, 10, img_scaled.shape).astype(np.uint8)
                img_scaled = cv2.add(img_scaled, noise)
                all_imgs.append(img_scaled)
                all_labels.append(digit)

    return np.array(all_imgs), np.array(all_labels)


def generate_synthetic_data(samples_per_digit=300):
    """生成合成数据，加噪声模拟真实场景"""
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

                    # 随机平移
                    tx = np.random.randint(-3, 4)
                    ty = np.random.randint(-3, 4)
                    M = np.float32([[1, 0, tx], [0, 1, ty]])
                    img = cv2.warpAffine(img, M, (IMG_SIZE, IMG_SIZE))

                    # 加噪声
                    noise = np.random.normal(0, 15, img.shape).astype(np.uint8)
                    img = cv2.add(img, noise)

                    all_imgs.append(img)
                    all_labels.append(digit)
                    count += 1
                if count >= samples_per_digit:
                    break
            if count >= samples_per_digit:
                break
        print(f"  数字 {digit}: 生成 {count} 张")

    return np.array(all_imgs), np.array(all_labels)


def build_cnn_model():
    """构建 CNN 模型"""
    model = Sequential([
        Conv2D(32, (3, 3), activation='relu', input_shape=(IMG_SIZE, IMG_SIZE, 1)),
        MaxPooling2D((2, 2)),
        Conv2D(64, (3, 3), activation='relu'),
        MaxPooling2D((2, 2)),
        Conv2D(128, (3, 3), activation='relu'),
        Flatten(),
        Dense(128, activation='relu'),
        Dropout(0.3),
        Dense(3, activation='softmax')
    ])
    model.compile(optimizer='adam', loss='categorical_crossentropy', metrics=['accuracy'])
    return model


def train_cnn_model():
    """训练 CNN，缓存到磁盘"""
    data_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'digit_data')

    # 判断是否需要重新训练：真实图片存在，且比已保存的模型更新
    need_retrain = False
    if os.path.isdir(data_dir) and os.path.exists(MODEL_PATH):
        model_mtime = os.path.getmtime(MODEL_PATH)
        for digit in TARGET_DIGITS:
            digit_dir = os.path.join(data_dir, str(digit))
            if not os.path.isdir(digit_dir):
                continue
            for fname in os.listdir(digit_dir):
                if not fname.lower().endswith(('.png', '.jpg', '.bmp')):
                    continue
                img_path = os.path.join(digit_dir, fname)
                if os.path.getmtime(img_path) > model_mtime:
                    need_retrain = True
                    print(f"检测到新图片: {img_path}，需要重新训练")
                    break
            if need_retrain:
                break

    if os.path.exists(MODEL_PATH) and not need_retrain:
        print(f"加载已保存的模型: {MODEL_PATH}")
        model = load_model(MODEL_PATH)
        print("模型加载成功，跳过训练")
        return model

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

    # 预处理：归一化到 0~1
    all_imgs = all_imgs.astype(np.float32) / 255.0
    all_imgs = np.expand_dims(all_imgs, axis=-1)  # (N, 32, 32, 1)

    # 标签映射
    label_map = {d: i for i, d in enumerate(TARGET_DIGITS)}
    all_labels_mapped = np.array([label_map[l] for l in all_labels])
    all_labels_oh = to_categorical(all_labels_mapped, num_classes=3)

    # 打乱并划分
    indices = np.random.permutation(len(all_labels))
    split = int(len(all_labels) * 0.8)
    train_idx, test_idx = indices[:split], indices[split:]

    x_train, y_train = all_imgs[train_idx], all_labels_oh[train_idx]
    x_test, y_test = all_imgs[test_idx], all_labels_oh[test_idx]

    print(f"训练集: {len(x_train)}, 测试集: {len(x_test)}")

    model = build_cnn_model()
    print("正在训练 CNN...")
    model.fit(x_train, y_train, epochs=20, batch_size=32, validation_split=0.1, verbose=1)

    # 测试
    loss, acc = model.evaluate(x_test, y_test, verbose=0)
    print(f"训练完成，测试准确率: {acc*100:.1f}%")

    # 保存
    model.save(MODEL_PATH)
    print(f"模型已保存: {MODEL_PATH}")

    return model


def robust_preprocess(img_bgr):
    """抗干扰预处理"""
    gray = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY)
    blur = cv2.GaussianBlur(gray, (3, 3), 0)
    bin_img = cv2.adaptiveThreshold(
        blur, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, cv2.THRESH_BINARY_INV, 15, 3
    )
    kernel = np.ones((2, 2), np.uint8)
    bin_img = cv2.morphologyEx(bin_img, cv2.MORPH_CLOSE, kernel)
    return bin_img


def extract_digit_roi(bin_img, min_w, max_w, min_h, max_h):
    """提取数字 ROI，带边缘留白"""
    contours, _ = cv2.findContours(bin_img, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    roi_list = []
    h_img, w_img = bin_img.shape

    for cnt in contours:
        x, y, w, h = cv2.boundingRect(cnt)
        if not (min_w < w < max_w and min_h < h < max_h):
            continue
        aspect_ratio = float(w) / h
        if not (0.3 <= aspect_ratio <= 0.8):
            continue
        x1 = max(0, x - PAD)
        y1 = max(0, y - PAD)
        x2 = min(w_img, x + w + PAD)
        y2 = min(h_img, y + h + PAD)
        roi = bin_img[y1:y2, x1:x2]
        roi_list.append((roi, x1, y1, x2, y2))

    return roi_list


def recognize_digit(model, roi):
    """用 CNN 识别数字"""
    roi_resized = cv2.resize(roi, (IMG_SIZE, IMG_SIZE))
    if np.mean(roi_resized) < 127:
        roi_resized = 255 - roi_resized
    roi_float = roi_resized.astype(np.float32) / 255.0
    roi_float = np.expand_dims(roi_float, axis=-1)  # (32, 32, 1)
    roi_float = np.expand_dims(roi_float, axis=0)   # (1, 32, 32, 1)

    pred = model.predict(roi_float, verbose=0)
    idx = int(np.argmax(pred[0]))
    reverse_map = {0: TARGET_DIGITS[0], 1: TARGET_DIGITS[1], 2: TARGET_DIGITS[2]}
    return reverse_map.get(idx, -1)


def main():
    import time

    print("TensorFlow 版本:", tf.__version__)

    # 先训练模型
    model = train_cnn_model()

    # 打开摄像头
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

    print("操作提示：按 q 关闭程序，拖动滑块实时调参")

    fps = 0
    frame_count = 0
    last_time = cv2.getTickCount()

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        min_w = cv2.getTrackbarPos("MinW", WIN_PARAMS)
        max_w = cv2.getTrackbarPos("MaxW", WIN_PARAMS)
        min_h = cv2.getTrackbarPos("MinH", WIN_PARAMS)
        max_h = cv2.getTrackbarPos("MaxH", WIN_PARAMS)

        if min_w >= max_w:
            max_w = min_w + 1
        if min_h >= max_h:
            max_h = min_h + 1

        binary = robust_preprocess(frame)
        roi_list = extract_digit_roi(binary, min_w, max_w, min_h, max_h)

        display = frame.copy()
        for roi, x1, y1, x2, y2 in roi_list:
            cv2.rectangle(display, (x1, y1), (x2, y2), (0, 255, 0), 2)
            digit = recognize_digit(model, roi)
            cv2.putText(display, str(digit), (x1, y1 - 6),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

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