#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
数据准备脚本
功能：
1. 从原始图片目录读取图像
2. 支持 LabelMe/COCO/YOLO 格式标注转换
3. 自动划分训练集/验证集
4. 数据增强（旋转、翻转、亮度、对比度、噪声）
5. 生成 YOLO 格式的 data.yaml

使用方式:
  python prepare_data.py                          # 使用默认配置
  python prepare_data.py --config config.yaml     # 指定配置文件
  python prepare_data.py --raw_dir ./my_images    # 指定原始图片目录
  python prepare_data.py --augment                # 启用数据增强
"""
# 命令行参数解析库
import argparse
# 日志打印模块，用于分级输出运行信息
import logging
# 系统路径操作、文件夹创建判断
import os
# 随机数模块，用于打乱数据集划分训练验证集
import random
# 文件复制、移动工具
import shutil
# 面向对象路径处理，替代传统os.path，更简洁
from pathlib import Path

# OpenCV 图像处理库，读取图片、图像变换、保存图片
import cv2
# 数值计算数组库，opencv图像底层基于numpy数组
import np

# 自定义配置加载类，外部文件 config_loader.py
from config_loader import PipelineConfig

# 日志全局配置：INFO级别日志，打印时间+日志等级+日志内容
logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
# 创建本脚本专属日志对象
logger = logging.getLogger(__name__)

# 支持读取的图片后缀集合，统一小写
SUPPORTED_IMG_EXTS = {'.jpg', '.jpeg', '.png', '.bmp', '.webp'}


class DataPreparer:
    """YOLO数据集数据准备器核心类
    封装完整数据处理流水线：收集图片->划分数据集->拷贝图片->标注转换->数据增强->生成yaml->统计输出
    """

    def __init__(self, config: PipelineConfig):
        """构造函数
        :param config: PipelineConfig 配置实例，包含项目路径、类别、增强参数、划分比例等
        """
        # 保存全局配置对象
        self.config = config
        # 数据集根输出目录，转为Path对象方便路径拼接
        self.base_dir = Path(config.project.base_dir)

    def prepare(self):
        """执行完整数据准备流水线入口主函数
        :return: bool True=全部流程执行成功，False=流程异常中断
        """
        logger.info(f"开始数据准备流程，当前配置信息: {self.config}")

        # 步骤1：遍历原始目录收集所有合法图片路径
        images = self._collect_images()
        # 图片列表为空直接终止流程
        if not images:
            logger.error(f"未在原始图片目录 {self.config.data.raw_dir} 找到任何图片文件")
            return False

        logger.info(f"成功收集原始图片总数：{len(images)} 张")

        # 步骤2：按比例随机划分训练集、验证集图片路径列表
        train_imgs, val_imgs = self._split_dataset(images)
        logger.info(f"数据集划分完成 -> 训练集: {len(train_imgs)} 张, 验证集: {len(val_imgs)} 张")

        # 步骤3：将原始图片拷贝到输出目录 images/train images/val
        self._copy_images(train_imgs, 'train')
        self._copy_images(val_imgs, 'val')

        # 步骤4：匹配每张图片对应的标注文件，统一转换成YOLO txt标注格式，保存到labels/train labels/val
        self._convert_annotations(train_imgs, 'train')
        self._convert_annotations(val_imgs, 'val')

        # 步骤5：判断配置是否开启数据增强，仅对训练集做图像增强
        if self.config.data.augment:
            self._augment_dataset()

        # 步骤6：根据配置自动生成YOLO训练所需data.yaml文件
        yaml_path = self.config.generate_data_yaml()
        logger.info(f"YOLO配置文件 data.yaml 生成完成，路径: {yaml_path}")

        # 步骤7：统计训练/验证集图片数量、标注数量、各类别目标数量并打印
        self._print_statistics()

        logger.info("所有数据处理流程全部执行完成！")
        return True

    def _collect_images(self) -> list:
        """私有方法：遍历原始图片目录，收集所有支持格式图片绝对路径
        :return: list[Path] 所有图片路径对象列表
        """
        # 读取配置里的原始图片根目录，转为Path对象
        raw_dir = Path(self.config.data.raw_dir)
        # 判断原始目录不存在
        if not raw_dir.exists():
            logger.warning(f"原始图片目录不存在: {raw_dir}，自动创建空目录")
            # parents=True 递归创建多层文件夹 exist_ok=True 存在不报错
            raw_dir.mkdir(parents=True, exist_ok=True)
            # 目录为空返回空列表
            return []

        # 存储所有图片路径
        images = []
        # 遍历所有支持的图片后缀
        for ext in SUPPORTED_IMG_EXTS:
            # 第一层目录匹配：当前目录下直接的图片
            images.extend(raw_dir.glob(f"*{ext}"))
            # 递归子目录匹配：所有子文件夹内图片
            images.extend(raw_dir.glob(f"**/*{ext}"))

        # set去重，防止同一张图片被重复匹配，转回list返回
        return list(set(images))

    def _split_dataset(self, images: list) -> tuple:
        """私有方法：随机打乱图片列表，按配置比例分割训练/验证集
        :param images: 全部原始图片路径列表
        :return: tuple(train列表, val列表)
        """
        # 原地随机打乱图片顺序，保证划分随机性
        random.shuffle(images)
        # 根据配置训练集比例计算训练集图片数量
        n_train = int(len(images) * self.config.data.train_ratio)
        # 前n_train张训练集，剩余全部验证集
        return images[:n_train], images[n_train:]

    def _copy_images(self, images: list, split: str):
        """私有方法：复制图片到输出数据集对应文件夹
        :param images: 需要拷贝的图片路径列表
        :param split: 数据集分段标识，'train' / 'val'
        """
        # 拼接目标图片文件夹路径：项目根目录/images/train 或 /images/val
        dest_dir = self.base_dir / "images" / split
        dest_dir.mkdir(parents=True, exist_ok=True)

        # 遍历每张图片执行拷贝
        for img_path in images:
            # 目标文件完整路径，保持原图片文件名
            dest = dest_dir / img_path.name
            # copy2保留文件原始属性（修改时间等），比普通copy更稳妥
            shutil.copy2(img_path, dest)

    def _convert_annotations(self, images: list, split: str):
        """私有方法：匹配图片对应的标注文件，转换为标准YOLO txt标注
        支持两种本地标注：LabelMe json / YOLO原生txt；COCO批量json单独预留逻辑
        :param images: 当前分段(train/val)的图片路径列表
        :param split: 数据集分段标识 train/val
        """
        # 原始标注存放目录和原始图片目录一致
        raw_dir = Path(self.config.data.raw_dir)
        # 输出YOLO标注存放目录 labels/train labels/val
        label_dir = self.base_dir / "labels" / split
        label_dir.mkdir(parents=True, exist_ok=True)

        # 遍历当前分段所有图片，匹配标注
        for img_path in images:
            # 获取图片文件名（不带后缀），标注文件和图片同名
            stem = img_path.stem
            ann_file = None

            # 优先级1：优先匹配LabelMe生成的json标注文件
            json_path = raw_dir / f"{stem}.json"
            if json_path.exists():
                ann_file = json_path
                # 执行LabelMe JSON转YOLO txt转换
                self._convert_labelme(json_path, label_dir / f"{stem}.txt", img_path)
                # 匹配成功跳过后续标注格式判断
                continue

            # 优先级2：匹配原生YOLO txt标注，无需转换直接复制
            txt_path = raw_dir / f"{stem}.txt"
            if txt_path.exists():
                ann_file = txt_path
                shutil.copy2(txt_path, label_dir / f"{stem}.txt")
                continue

            # 未匹配到json/txt标注，跳过当前图片
            if not ann_file:
                logger.debug(f"图片 {stem} 未匹配到任何标注文件，跳过该图")

    def _convert_labelme(self, json_path, dest_txt, img_path):
        """私有方法：LabelMe标注JSON文件 转换为 YOLO标准txt标注
        支持矩形框rectangle、多边形polygon（自动取外接矩形）
        YOLO标注格式：类别ID 归一化中心x 归一化中心y 归一化框宽 归一化框高
        :param json_path: LabelMe json文件路径
        :param dest_txt: 转换后输出的YOLO txt路径
        :param img_path: json对应的原图路径，用于获取图像宽高做归一化
        """
        try:
            # 内置json模块读取标注文件
            import json
            with open(json_path, 'r', encoding='utf-8') as f:
                data = json.load(f)

            # opencv读取原图
            img = cv2.imread(str(img_path))
            # 图片读取失败直接退出转换
            if img is None:
                return
            # 获取图片高度、宽度（忽略通道数）
            h, w = img.shape[:2]

            # 存储转换完成后的标注行文本
            lines = []
            # 遍历json内所有标注框
            for shape in data.get('shapes', []):
                label = shape['label']
                # 标签不在配置定义类别列表内，直接忽略该目标
                if label not in self.config.class_names:
                    continue

                # 根据类别名称获取对应数字ID（YOLO要求数字类别）
                class_id = self.config.class_names.index(label)
                # 获取标注点坐标数组
                points = shape['points']

                # 判断标注框类型
                if shape['shape_type'] == 'rectangle':
                    # 矩形框：两个对角点
                    x1, y1 = points[0]
                    x2, y2 = points[1]
                elif shape['shape_type'] == 'polygon':
                    # 多边形：取所有点最大最小坐标生成外接矩形
                    xs = [p[0] for p in points]
                    ys = [p[1] for p in points]
                    x1, y1 = min(xs), min(ys)
                    x2, y2 = max(xs), max(ys)
                else:
                    # 不支持的标注类型（点、线等）直接跳过
                    continue

                # 计算YOLO归一化参数：除以原图宽高映射到0~1区间
                # 框中心点x
                cx = (x1 + x2) / 2 / w
                # 框中心点y
                cy = (y1 + y2) / 2 / h
                # 框宽度
                bw = (x2 - x1) / w
                # 框高度
                bh = (y2 - y1) / h

                # 保留6位小数，拼接单行标注文本
                lines.append(f"{class_id} {cx:.6f} {cy:.6f} {bw:.6f} {bh:.6f}")

            # 将所有标注行写入目标txt文件
            with open(dest_txt, 'w', encoding='utf-8') as f:
                f.write('\n'.join(lines))

        except Exception as e:
            # 转换出现任何异常捕获并打印日志，不中断整个程序
            logger.error(f"LabelMe转换失败 文件:{json_path} 错误信息:{e}")

    def _augment_dataset(self):
        """私有方法：训练集数据增强逻辑，仅对train集生效
        支持：水平翻转、垂直翻转、亮度对比度随机调整
        增强后同步修改对应标注坐标，保存带后缀的新图片+标注
        """
        # 读取增强配置参数
        aug_config = self.config.augmentation
        # 训练集图片、标注目录路径
        train_img_dir = self.base_dir / "images" / "train"
        train_label_dir = self.base_dir / "labels" / "train"

        # 获取训练集所有图片路径
        images = list(train_img_dir.glob("*"))
        # 统计增强生成图片总数
        aug_count = 0

        # 遍历每张训练原图做增强
        for img_path in images:
            img = cv2.imread(str(img_path))
            # 图片读取失败跳过
            if img is None:
                continue

            # 匹配对应标注文件
            label_path = train_label_dir / f"{img_path.stem}.txt"
            # 无标注图片不做增强
            if not label_path.exists():
                continue

            # 读取原始标注文本字符串
            with open(label_path, 'r') as f:
                original_labels = f.read().strip()

            # 增强1：水平左右翻转
            if aug_config.flip_lr:
                aug_img = cv2.flip(img, 1)
                # 翻转标注坐标
                aug_labels = self._flip_labels_horizontal(original_labels)
                # 保存增强图片+标注，后缀flr区分
                self._save_augmented(
                    train_img_dir, train_label_dir,
                    img_path.stem, aug_img, aug_labels, "flr"
                )
                aug_count += 1

            # 增强2：垂直上下翻转
            if aug_config.flip_ud:
                aug_img = cv2.flip(img, 0)
                aug_labels = self._flip_labels_vertical(original_labels)
                self._save_augmented(
                    train_img_dir, train_label_dir,
                    img_path.stem, aug_img, aug_labels, "fud"
                )
                aug_count += 1

            # 增强3：随机亮度、对比度调整
            if aug_config.brightness > 0 or aug_config.contrast > 0:
                # alpha对比度系数：1为原图，随机上下浮动
                alpha = 1.0 + random.uniform(-aug_config.contrast, aug_config.contrast)
                # beta亮度偏移，转换为像素偏移量
                beta = random.uniform(-aug_config.brightness * 255, aug_config.brightness * 255)
                # 图像像素映射变换
                aug_img = cv2.convertScaleAbs(img, alpha=alpha, beta=beta)
                # 亮度对比度不改变框坐标，直接复用原标注
                self._save_augmented(
                    train_img_dir, train_label_dir,
                    img_path.stem, aug_img, original_labels, "bc"
                )
                aug_count += 1

        logger.info(f"训练集数据增强全部完成，共新增增强图片 {aug_count} 张")

    def _flip_labels_horizontal(self, labels_str: str) -> str:
        """私有工具：水平翻转后修正YOLO标注坐标
        水平翻转只修改中心x坐标：cx = 1 - cx
        :param labels_str: 原始标注完整文本
        :return: 翻转后新标注文本
        """
        # 按换行分割所有标注框
        lines = labels_str.strip().split('\n')
        new_lines = []
        for line in lines:
            parts = line.split()
            # 标准YOLO一行固定5个字段才处理
            if len(parts) == 5:
                class_id = parts[0]
                # 水平翻转x中心取镜像
                cx = 1.0 - float(parts[1])
                cy = float(parts[2])
                bw = parts[3]
                bh = parts[4]
                new_lines.append(f"{class_id} {cx:.6f} {cy:.6f} {bw} {bh}")
        # 拼接回完整标注字符串
        return '\n'.join(new_lines)

    def _flip_labels_vertical(self, labels_str: str) -> str:
        """私有工具：垂直翻转后修正YOLO标注坐标
        垂直翻转只修改中心y坐标：cy = 1 - cy
        :param labels_str: 原始标注完整文本
        :return: 翻转后新标注文本
        """
        lines = labels_str.strip().split('\n')
        new_lines = []
        for line in lines:
            parts = line.split()
            if len(parts) == 5:
                class_id = parts[0]
                cx = float(parts[1])
                # 垂直翻转y中心镜像
                cy = 1.0 - float(parts[2])
                bw = parts[3]
                bh = parts[4]
                new_lines.append(f"{class_id} {cx:.6f} {cy:.6f} {bw} {bh}")
        return '\n'.join(new_lines)

    def _save_augmented(self, img_dir, label_dir, stem, img, labels, suffix):
        """私有工具：统一保存增强后的图片和对应标注文件
        :param img_dir: 图片输出目录
        :param label_dir: 标注输出目录
        :param stem: 原图基础文件名（无后缀）
        :param img: opencv图像数组（增强后）
        :param labels: 修改完成的标注文本
        :param suffix: 增强类型后缀，flr/fud/bc，区分原图
        """
        # 增强图片文件名：原图名_后缀.jpg
        img_path = img_dir / f"{stem}_{suffix}.jpg"
        # 增强标注文件名：原图名_后缀.txt
        label_path = label_dir / f"{stem}_{suffix}.txt"

        # 保存图片，默认jpg格式
        cv2.imwrite(str(img_path), img)
        # 写入标注文本
        with open(label_path, 'w') as f:
            f.write(labels)

    def _print_statistics(self):
        """私有方法：数据集统计打印
        分别统计train/val：图片总数、标注文件总数、每个类别目标框总数量
        """
        # 分别遍历训练集、验证集
        for split in ['train', 'val']:
            img_dir = self.base_dir / "images" / split
            label_dir = self.base_dir / "labels" / split

            # 统计图片数量，目录不存在则0
            n_imgs = len(list(img_dir.glob("*"))) if img_dir.exists() else 0
            # 统计标注txt文件数量
            n_labels = len(list(label_dir.glob("*.txt"))) if label_dir.exists() else 0

            # 初始化类别计数字典，所有类别初始数量0
            class_counts = {name: 0 for name in self.config.class_names}
            if label_dir.exists():
                # 遍历所有标注txt统计目标框
                for txt in label_dir.glob("*.txt"):
                    with open(txt, 'r') as f:
                        for line in f:
                            parts = line.strip().split()
                            # 非空标注行
                            if parts:
                                cid = int(parts[0])
                                # 类别ID合法才计数
                                if cid < len(self.config.class_names):
                                    class_counts[self.config.class_names[cid]] += 1

            # 格式化打印统计信息
            logger.info(f"\n{'='*40}")
            logger.info(f"{split.upper()} 数据集统计信息:")
            logger.info(f"  图片总数量: {n_imgs}")
            logger.info(f"  标注文件数量: {n_labels}")
            logger.info(f"  各类别目标实例数量:")
            for name, count in class_counts.items():
                logger.info(f"    {name}: {count}")


def parse_args():
    """解析命令行启动时传入的参数
    :return: 命令行参数对象 args
    """
    # 创建参数解析器，添加脚本描述
    p = argparse.ArgumentParser(description='YOLO目标检测数据集自动化数据准备脚本')
    # 指定yaml配置文件路径
    p.add_argument('--config', default=None, help='外部配置yaml文件路径')
    # 覆盖原始图片目录
    p.add_argument('--raw_dir', default=None, help='原始图片存放目录，命令行传入会覆盖配置文件')
    # 数据集输出根目录
    p.add_argument('--base_dir', default=None, help='处理完成的数据集输出根目录')
    # 布尔参数，传入即开启数据增强
    p.add_argument('--augment', action='store_true', help='开关参数，添加则启用训练集数据增强')
    # 手动指定训练集划分比例
    p.add_argument('--train_ratio', type=float, default=None, help='训练集占总数据比例，0~1浮点数')
    return p.parse_args()


def main():
    """程序主入口函数
    加载配置 -> 命令行参数覆盖配置 -> 初始化处理类 -> 执行流水线
    :return: int 0正常退出，1异常退出
    """
    # 解析终端传入参数
    args = parse_args()

    # 加载配置文件，无--config则加载默认配置
    config = PipelineConfig(args.config)

    # 命令行参数优先级高于yaml配置，覆盖对应字段
    if args.raw_dir:
        config.data.raw_dir = args.raw_dir
    if args.base_dir:
        config.project.base_dir = args.base_dir
    if args.augment:
        config.data.augment = True
    if args.train_ratio:
        config.data.train_ratio = args.train_ratio

    # 实例化数据处理器
    preparer = DataPreparer(config)
    # 执行完整数据准备流程
    success = preparer.prepare()

    # 成功返回0，失败返回1，给操作系统进程退出码
    return 0 if success else 1


if __name__ == "__main__":
    # 脚本直接运行时才执行main函数，导入为模块不执行
    import sys
    # 接收main返回码退出程序
    sys.exit(main())