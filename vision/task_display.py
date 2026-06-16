"""
任务码显示装置模块
比赛规则：搬运机器人必须配备任务码显示装置，放置在醒目位置，
亮光显示，字体高度不小于12mm，能显示所有任务码及完成情况
"""
import cv2
import numpy as np

from .color_detector import COLOR_ID_MAP
from .qr_detector import TaskCodeParser


class TaskDisplay:
    """
    任务码显示装置生成器
    黑底亮色字体显示，模拟比赛现场的LED/OLED显示
    """
    def __init__(self, width=400, height=200):
        self.width = width
        self.height = height
        self.parser = TaskCodeParser()

    def render(self, task_code, completed_steps=None):
        """
        渲染任务码显示图
        :param task_code: 任务码字符串 如 "156+123+516+231"
        :param completed_steps: 已完成步骤列表 [True, False, ...]
        :return: BGR格式显示图
        """
        display = np.zeros((self.height, self.width, 3), dtype=np.uint8)
        display[:] = (0, 0, 0)

        # 标题
        cv2.putText(display, "TASK CODE", (10, 25),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 255), 2)

        # 任务码 (大字体，模拟12mm字高)
        if task_code:
            cv2.putText(display, task_code, (10, 80),
                        cv2.FONT_HERSHEY_SIMPLEX, 1.4, (0, 255, 0), 3)
            # 解析并显示含义
            parsed = self.parser.parse(task_code)
            if parsed:
                b1c, b1p, b2c, b2p = parsed
                b1c_names = [COLOR_ID_MAP[c][1] for c in b1c]
                b2c_names = [COLOR_ID_MAP[c][1] for c in b2c]
                info1 = f"B1:{b1c_names[0]}/{b1c_names[1]}/{b1c_names[2]} -> R{b1p[0]}{b1p[1]}{b1p[2]}"
                info2 = f"B2:{b2c_names[0]}/{b2c_names[1]}/{b2c_names[2]} -> R{b2p[0]}{b2p[1]}{b2p[2]}"
                cv2.putText(display, info1, (10, 115),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)
                cv2.putText(display, info2, (10, 140),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)

        # 完成进度
        if completed_steps is not None:
            progress = sum(completed_steps)
            total = len(completed_steps) if completed_steps else 1
            pct = int(progress / total * 100)
            cv2.putText(display, f"PROGRESS: {progress}/{total} ({pct}%)", (10, 175),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 2)
            # 进度条
            bar_w = self.width - 20
            fill_w = int(bar_w * pct / 100)
            cv2.rectangle(display, (10, 185), (10 + bar_w, 195), (50, 50, 50), -1)
            cv2.rectangle(display, (10, 185), (10 + fill_w, 195), (0, 255, 0), -1)

        return display
