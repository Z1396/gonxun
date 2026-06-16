#!/usr/bin/env python3
# 指定使用python3解释器运行该脚本
# -*- coding: utf-8 -*-
# 声明文件编码为UTF-8，支持中文注释不出现乱码
"""
测试图像生成器 - 用于演示工创赛视觉系统
功能：批量生成4种模拟赛场图片，离线调试前面的视觉识别代码，不用外接摄像头
1. 三色物料色块图（对应mode=1颜色识别）
2. 三色定位色环图（对应mode=3霍夫圆检测）
3. 二维码识别图（对应mode=9二维码解码）
4. 综合仿真实景图（模拟真实赛场光照、阴影、噪点）
"""

# 导入依赖库
import cv2          # OpenCV，绘制圆形、叠加图像、保存图片
import numpy as np  # 矩阵生成画布、随机噪点、渐变光照计算
import os           # 文件系统操作，创建存储图片的文件夹

def create_color_blocks_image():
    """
    创建包含红绿蓝三色实心物料圆块的测试图像
    用途：单独调试color_blocks_position_WL颜色色块识别函数
    返回值：生成完成的图像矩阵img
    """
    # 创建480行高、640列宽、3通道彩色空白画布，像素数据类型无符号8位
    img = np.zeros((480, 640, 3), dtype=np.uint8)
    
    # 整张画布填充浅灰白色背景
    img[:] = (240, 240, 240)
    
    # 绘制红色物料圆，圆心(200,200)，半径50，BGR红色(0,0,255)，-1代表实心填充
    cv2.circle(img, (200, 200), 50, (0, 0, 255), -1)
    # 绿色物料圆
    cv2.circle(img, (320, 200), 50, (0, 255, 0), -1)
    # 蓝色物料圆
    cv2.circle(img, (440, 200), 50, (255, 0, 0), -1)
    
    # 生成高斯随机噪声，均值0，标准差10，尺寸和原图完全一致
    noise = np.random.normal(0, 10, img.shape).astype(np.uint8)
    # 原图叠加噪声，模拟摄像头画面噪点，模拟现场光线干扰
    img = cv2.add(img, noise)
    
    # 将生成图片保存到当前目录
    cv2.imwrite('test_color_blocks.jpg', img)
    # 返回图像矩阵供外部调用
    return img

def create_color_circles_image():
    """
    创建包含三个空心定位色环的测试图像
    用途：单独调试color_circle_position霍夫圆检测函数
    返回值：生成完成的图像矩阵img
    """
    # 初始化480*640三通道画布
    img = np.zeros((480, 640, 3), dtype=np.uint8)
    
    # 填充浅灰色背景
    img[:] = (200, 200, 200)
    
    # 定义三种圆环颜色、对应圆心坐标
    colors = [(0, 0, 255), (0, 255, 0), (255, 0, 0)]
    positions = [(180, 240), (320, 240), (460, 240)]
    
    # zip同时遍历颜色列表和坐标列表，批量绘制空心圆环
    for color, pos in zip(colors, positions):
        # 半径40，线条宽度8，正数代表空心圆环
        cv2.circle(img, pos, 40, color, 8)
    
    # 复制原图作为光照遮罩图层
    overlay = img.copy()
    # 在画面中心绘制白色圆形光斑，模拟现场顶光照明
    cv2.circle(overlay, (320, 240), 200, (255, 255, 255), -1)
    # 图层加权融合：光斑图层透明度0.1，原图透明度0.9，模拟柔和光照
    img = cv2.addWeighted(overlay, 0.1, img, 0.9, 0)
    
    # 保存色环测试图
    cv2.imwrite('test_color_circles.jpg', img)
    return img

def create_qr_code_image():
    """
    创建带有二维码的测试图像
    用途：单独调试detect_qr_code二维码识别函数
    返回值：生成完成的图像矩阵img
    """
    # 本地导入二维码生成库，仅本函数需要，写内部避免全局多余依赖
    import qrcode
    
    # 初始化二维码生成器：版本1最小尺寸，每个方块10像素，白边5格
    qr = qrcode.QRCode(version=1, box_size=10, border=5)
    # 二维码存储文本内容：物料编号ABC123
    qr.add_data('ABC123')
    # 自动适配内容尺寸生成二维码矩阵
    qr.make(fit=True)
    
    # 生成PIL图像对象，黑色码、白色背景
    qr_img = qr.make_image(fill_color="black", back_color="white")
    # PIL图像转为numpy RGB矩阵，适配OpenCV处理格式
    qr_img = np.array(qr_img.convert('RGB'))
    
    # 缩放二维码到200×200像素
    qr_img = cv2.resize(qr_img, (200, 200))
    
    # 创建纯白背景画布
    img = np.ones((480, 640, 3), dtype=np.uint8) * 255
    
    # 二维码放置左上角偏移坐标
    y_offset = 140
    x_offset = 220
    # 将二维码矩阵粘贴到画布指定区域
    img[y_offset:y_offset+200, x_offset:x_offset+200] = qr_img
    
    # 保存二维码测试图片
    cv2.imwrite('test_qr_code.jpg', img)
    return img

def create_realistic_scene():
    """
    创建高度仿真的综合赛场场景
    同时包含三色物料、定位色环、阴影渐变光照、模拟赛场环境
    用途：完整整套视觉算法联合调试，最贴近真实摄像头画面
    返回值：生成完成的图像矩阵img
    """
    # 创建空白画布
    img = np.zeros((480, 640, 3), dtype=np.uint8)
    
    # 随机生成180~220之间数值，模拟不均匀灰色场地背景
    background = np.random.randint(180, 220, (480, 640, 3), dtype=np.uint8)
    
    # 逐行生成垂直渐变光照，画面中间亮、上下边缘变暗
    for i in range(480):
        # 计算当前行亮度系数，中心行系数最高0.7，上下两端最低0.3
        alpha = 0.3 + 0.4 * (1 - abs(i - 240) / 240)
        # 当前行所有像素乘以亮度系数，转无符号整数
        background[i] = (background[i] * alpha).astype(np.uint8)
    
    # 画布赋值渐变背景
    img = background
    
    # ========== 绘制三个带黑色描边的物料块 ==========
    # 红色物料实心圆
    cv2.circle(img, (180, 160), 35, (0, 0, 255), -1)
    # 黑色外边框，模拟实物物料轮廓
    cv2.circle(img, (180, 160), 35, (0, 0, 0), 2)
    
    # 绿色物料
    cv2.circle(img, (320, 180), 35, (0, 255, 0), -1)
    cv2.circle(img, (320, 180), 35, (0, 0, 0), 2)
    
    # 蓝色物料
    cv2.circle(img, (460, 200), 35, (255, 0, 0), -1)
    cv2.circle(img, (460, 200), 35, (0, 0, 0), 2)
    
    # ========== 绘制三个定位空心色环 ==========
    cv2.circle(img, (150, 350), 30, (0, 0, 255), 6)
    cv2.circle(img, (320, 370), 30, (0, 255, 0), 6)
    cv2.circle(img, (490, 350), 30, (255, 0, 0), 6)
    
    # ========== 添加物料底部阴影效果 ==========
    # 复制原图作为阴影图层
    shadow = img.copy()
    # 在物料右下偏移位置绘制黑色圆形阴影
    cv2.circle(shadow, (185, 165), 35, (0, 0, 0), -1)
    cv2.circle(shadow, (325, 185), 35, (0, 0, 0), -1)
    cv2.circle(shadow, (465, 205), 35, (0, 0, 0), -1)
    # 阴影图层透明度0.2叠加原图，模拟实物投影
    img = cv2.addWeighted(shadow, 0.2, img, 0.8, 0)
    
    # 保存综合仿真场景图
    cv2.imwrite('test_realistic.jpg', img)
    return img

# 程序入口，脚本直接运行时执行下方逻辑
if __name__ == "__main__":
    print("正在生成测试图像...")
    
    # 判断test_images文件夹是否存在
    if not os.path.exists('test_images'):
        # 不存在则创建文件夹存放图片
        os.makedirs('test_images')
    
    # 依次调用四个生成函数，产出四张测试图片
    create_color_blocks_image()
    create_color_circles_image()
    create_qr_code_image()
    create_realistic_scene()
    
    # 控制台打印完成提示与每张图片对应功能说明
    print("测试图像生成完成！")
    print("生成了以下测试图像：")
    print("- test_color_blocks.jpg (三色块识别)")
    print("- test_color_circles.jpg (色环定位)")
    print("- test_qr_code.jpg (二维码识别)")
    print("- test_realistic.jpg (综合场景)")