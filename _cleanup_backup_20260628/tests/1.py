import cv2
import numpy as np
import tensorrt as trt
import ctypes

# ====================== 全局配置 ======================
WIN_PARAMS = "Param_Tune"
INPUT_SIZE = (32, 32)  # CNN输入尺寸
CLASS_MAP = {0: 1, 1: 2, 2: 3}  # 网络输出映射真实数字
TRT_ENGINE_PATH = "num_3class.engine"  # 训练后导出的TensorRT引擎

# 加载TensorRT引擎
def load_trt_engine(engine_path):
    TRT_LOGGER = trt.Logger(trt.Logger.WARNING)
    with open(engine_path, "rb") as f, trt.Runtime(TRT_LOGGER) as runtime:
        return runtime.deserialize_cuda_engine(f.read())

# 推理初始化
TRT_ENGINE = load_trt_engine(TRT_ENGINE_PATH)
TRT_CONTEXT = TRT_ENGINE.create_execution_context()
# 分配GPU/CPU内存
input_shape = TRT_ENGINE.get_tensor_shape("input")
output_shape = TRT_ENGINE.get_tensor_shape("output")
host_input = np.empty(input_shape, dtype=np.float32)
host_output = np.empty(output_shape, dtype=np.float32)
# CUDA显存指针
import pycuda.driver as cuda
import pycuda.autoinit
d_input = cuda.mem_alloc(host_input.nbytes)
d_output = cuda.mem_alloc(host_output.nbytes)
bindings = [int(d_input), int(d_output)]

def infer_single_num(img_gray_32):
    """单张32×32灰度数字图推理，返回识别数字 1/2/3"""
    # 归一化 0~255 -> 0~1
    img_norm = img_gray_32.astype(np.float32) / 255.0
    host_input[0,0,:,:] = img_norm
    # 拷贝到GPU
    cuda.memcpy_htod(d_input, host_input)
    # 推理
    TRT_CONTEXT.execute_v2(bindings)
    # 取回结果
    cuda.memcpy_dtoh(host_output, d_output)
    pred_idx = np.argmax(host_output)
    return CLASS_MAP[pred_idx]

# ====================== 图像预处理流水线 ======================
def robust_preprocess(img_bgr):
    gray = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2GRAY)
    blur = cv2.GaussianBlur(gray, (3, 3), 0)
    # 自适应阈值抗反光、远距离明暗不均
    bin_img = cv2.adaptiveThreshold(
        blur, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, cv2.THRESH_BINARY_INV, 15, 3
    )
    # 闭运算修复远距离断裂笔画
    kernel = np.ones((2, 2), np.uint8)
    bin_img = cv2.morphologyEx(bin_img, cv2.MORPH_CLOSE, kernel)
    return bin_img

# 提取数字轮廓ROI
def extract_num_roi(bin_img, min_w, max_w, min_h, max_h):
    contours, _ = cv2.findContours(bin_img, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    roi_list = []
    box_list = []
    h_img, w_img = bin_img.shape
    pad = 3  # 轮廓边缘留白，防止裁剪丢失笔画

    for cnt in contours:
        x, y, w, h = cv2.boundingRect(cnt)
        # 滑动条动态尺寸过滤噪点
        if not (min_w < w < max_w and min_h < h < max_h):
            continue
        # 边界保护
        x1 = max(0, x - pad)
        y1 = max(0, y - pad)
        x2 = min(w_img, x + w + pad)
        y2 = min(h_img, y + h + pad)
        roi = bin_img[y1:y2, x1:x2]
        roi_resized = cv2.resize(roi, INPUT_SIZE)
        roi_list.append(roi_resized)
        box_list.append((x1, y1, x2, y2))
    return roi_list, box_list

# ====================== 主采集识别逻辑 ======================
def main():
    # 摄像头初始化 Jetson V4L2
    cap = cv2.VideoCapture(1, cv2.CAP_V4L2)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc('M','J','P','G'))
    cap.set(cv2.CAP_PROP_FPS, 25)
    # Linux手动曝光防频闪
    cap.set(cv2.CAP_PROP_AUTO_EXPOSURE, 1)
    cap.set(cv2.CAP_PROP_EXPOSURE, 120)
    cap.set(cv2.CAP_PROP_GAIN, 15)

    # 创建调参滑动条窗口
    cv2.namedWindow(WIN_PARAMS)
    cv2.createTrackbar("Threshold", WIN_PARAMS, 127, 255, lambda x: None)
    cv2.createTrackbar("MinW", WIN_PARAMS, 12, 80, lambda x: None)
    cv2.createTrackbar("MaxW", WIN_PARAMS, 60, 120, lambda x: None)
    cv2.createTrackbar("MinH", WIN_PARAMS, 18, 90, lambda x: None)
    cv2.createTrackbar("MaxH", WIN_PARAMS, 70, 130, lambda x: None)

    # FPS统计变量
    frame_count = 0
    fps = 0.0
    last_tick = cv2.getTickCount()
    tick_freq = cv2.getTickFrequency()

    print("数字识别程序启动，仅识别1/2/3，50cm稳定识别，q退出")
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        draw_frame = frame.copy()

        # 读取滑动条参数
        min_w = cv2.getTrackbarPos("MinW", WIN_PARAMS)
        max_w = cv2.getTrackbarPos("MaxW", WIN_PARAMS)
        min_h = cv2.getTrackbarPos("MinH", WIN_PARAMS)
        max_h = cv2.getTrackbarPos("MaxH", WIN_PARAMS)

        # 预处理二值图
        binary = robust_preprocess(frame)
        # 提取数字ROI
        roi_imgs, boxes = extract_num_roi(binary, min_w, max_w, min_h, max_h)

        # 逐框推理识别
        for roi, (x1, y1, x2, y2) in zip(roi_imgs, boxes):
            num = infer_single_num(roi)
            # 绘制框+识别文字
            cv2.rectangle(draw_frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
            cv2.putText(draw_frame, f"{num}", (x1, y1 - 6),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

        # FPS计算绘制
        frame_count += 1
        curr_tick = cv2.getTickCount()
        elapsed = (curr_tick - last_tick) / tick_freq
        if elapsed >= 1.0:
            fps = frame_count / elapsed
            frame_count = 0
            last_tick = curr_tick
        cv2.putText(draw_frame, f"FPS:{fps:.1f}", (draw_frame.shape[1]-110, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 200, 255), 2)

        cv2.imshow("Num Detect", draw_frame)
        cv2.imshow("Binary View", binary)

        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()