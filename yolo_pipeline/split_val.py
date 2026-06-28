"""
从 train 中随机划分 20% 图片+标注到 val
"""
import os
import random
import shutil
from pathlib import Path

BASE = Path(__file__).parent.parent / "yolo_dataset"
IMG_TRAIN = BASE / "images" / "train"
LBL_TRAIN = BASE / "labels" / "train"
IMG_VAL = BASE / "images" / "val"
LBL_VAL = BASE / "labels" / "val"

VAL_RATIO = 0.2

# 收集有对应标注的图片
pairs = []
for img in IMG_TRAIN.glob("*.jpg"):
    lbl = LBL_TRAIN / f"{img.stem}.txt"
    if lbl.exists():
        pairs.append((img, lbl))

random.shuffle(pairs)
n_val = int(len(pairs) * VAL_RATIO)
val_pairs = pairs[:n_val]

IMG_VAL.mkdir(parents=True, exist_ok=True)
LBL_VAL.mkdir(parents=True, exist_ok=True)

for img, lbl in val_pairs:
    shutil.move(str(img), str(IMG_VAL / img.name))
    shutil.move(str(lbl), str(LBL_VAL / lbl.name))

print(f"总图片数: {len(pairs)}, 划分到 val: {len(val_pairs)}, train 剩余: {len(pairs) - len(val_pairs)}")
