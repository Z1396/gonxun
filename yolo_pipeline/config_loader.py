"""
YOLO 物块识别流程 - 配置加载器
从 config.yaml 加载配置参数，提供统一的配置接口
"""
import os
import yaml
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Optional


@dataclass
class ProjectConfig:
    name: str = "material_detection"
    base_dir: str = "./yolo_dataset"
    output_dir: str = "./runs"


@dataclass
class DataConfig:
    raw_dir: str = "./raw_images"
    train_ratio: float = 0.8
    val_ratio: float = 0.2
    img_size: List[int] = field(default_factory=lambda: [640, 640])
    augment: bool = True


@dataclass
class AugmentationConfig:
    rotation: int = 15
    flip_lr: bool = True
    flip_ud: bool = False
    brightness: float = 0.2
    contrast: float = 0.2
    noise: float = 0.05


@dataclass
class TrainingConfig:
    model: str = "yolov8n.pt"
    epochs: int = 100
    batch_size: int = 16
    imgsz: int = 640
    device: int = 0
    workers: int = 8
    lr0: float = 0.01
    lrf: float = 0.01
    patience: int = 50
    save_period: int = 10


@dataclass
class EvaluationConfig:
    conf_threshold: float = 0.5
    iou_threshold: float = 0.45
    target_map50: float = 0.90
    target_map: float = 0.75


@dataclass
class InferenceConfig:
    model_path: str = ""
    source: str = "0"
    conf_threshold: float = 0.5
    save: bool = True
    show: bool = True
    device: int = 0


class PipelineConfig:
    """YOLO物块识别流程配置管理器"""

    def __init__(self, config_path: str = None):
        if config_path is None:
            config_path = Path(__file__).parent / "config.yaml"

        self.config_path = Path(config_path)
        self.raw = {}

        # 默认配置
        self.project = ProjectConfig()
        self.classes_count = 6
        self.class_names = [
            "red_block", "blue_block", "green_block",
            "yellow_block", "black_block", "light_blue_block"
        ]
        self.data = DataConfig()
        self.augmentation = AugmentationConfig()
        self.training = TrainingConfig()
        self.evaluation = EvaluationConfig()
        self.inference = InferenceConfig()

        # 加载配置文件
        if self.config_path.exists():
            self.load()

    def load(self):
        """从YAML文件加载配置"""
        with open(self.config_path, 'r', encoding='utf-8') as f:
            self.raw = yaml.safe_load(f) or {}

        # 解析各模块配置
        if 'project' in self.raw:
            self.project = ProjectConfig(**self.raw['project'])

        if 'classes' in self.raw:
            self.classes_count = self.raw['classes'].get('count', 6)
            self.class_names = self.raw['classes'].get('names', self.class_names)

        if 'data' in self.raw:
            self.data = DataConfig(**{
                k: v for k, v in self.raw['data'].items()
                if k in DataConfig.__dataclass_fields__
            })

        if 'augmentation' in self.raw:
            self.augmentation = AugmentationConfig(**{
                k: v for k, v in self.raw['augmentation'].items()
                if k in AugmentationConfig.__dataclass_fields__
            })

        if 'training' in self.raw:
            self.training = TrainingConfig(**{
                k: v for k, v in self.raw['training'].items()
                if k in TrainingConfig.__dataclass_fields__
            })

        if 'evaluation' in self.raw:
            self.evaluation = EvaluationConfig(**{
                k: v for k, v in self.raw['evaluation'].items()
                if k in EvaluationConfig.__dataclass_fields__
            })

        if 'inference' in self.raw:
            self.inference = InferenceConfig(**{
                k: v for k, v in self.raw['inference'].items()
                if k in InferenceConfig.__dataclass_fields__
            })

    def get_data_yaml_path(self) -> str:
        """生成YOLO data.yaml 文件路径"""
        return os.path.join(self.project.base_dir, "data.yaml")

    def generate_data_yaml(self):
        """生成YOLO训练所需的data.yaml文件"""
        data_yaml = {
            'path': os.path.abspath(self.project.base_dir),
            'train': 'images/train',
            'val': 'images/val',
            'nc': self.classes_count,
            'names': self.class_names
        }

        yaml_path = self.get_data_yaml_path()
        os.makedirs(self.project.base_dir, exist_ok=True)

        with open(yaml_path, 'w', encoding='utf-8') as f:
            yaml.dump(data_yaml, f, default_flow_style=False, allow_unicode=True)

        return yaml_path

    def get_latest_model(self) -> Optional[str]:
        """获取最新训练模型路径"""
        if self.inference.model_path:
            return self.inference.model_path

        # 查找最新训练结果
        runs_dir = Path(self.project.output_dir) / "detect"
        if not runs_dir.exists():
            return None

        exp_dirs = sorted(runs_dir.glob("exp*"), key=lambda x: x.stat().st_mtime, reverse=True)
        if not exp_dirs:
            return None

        best_pt = exp_dirs[0] / "weights" / "best.pt"
        return str(best_pt) if best_pt.exists() else None

    def __repr__(self):
        return (f"PipelineConfig(classes={self.classes_count}, "
                f"model={self.training.model}, epochs={self.training.epochs})")
