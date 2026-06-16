"""
二维码识别与任务码解析模块
- 二维码识别
- 任务码格式校验与解析 (XXX+XXX+XXX+XXX)
"""
import cv2


class QRDetector:
    """
    二维码识别器
    """
    def __init__(self):
        self.decoder = cv2.QRCodeDetector()

    def detect(self, img):
        """
        识别二维码
        :param img: 输入图像
        :return: 二维码字符串数据；无二维码返回None
        """
        if img is None:
            return None
        data, bbox, _ = self.decoder.detectAndDecode(img)
        if data and bbox is not None:
            # 绘制二维码边框
            n = len(bbox[0])
            for j in range(n):
                p1 = tuple(bbox[0][j])
                p2 = tuple(bbox[0][(j + 1) % n])
                cv2.line(img, p1, p2, (255, 0, 0), 3)

            # 绘制中心点
            xs = [point[0] for point in bbox[0]]
            ys = [point[1] for point in bbox[0]]
            center_x = int(sum(xs) / 4)
            center_y = int(sum(ys) / 4)
            cv2.circle(img, (center_x, center_y), 5, (0, 255, 0), -1)
            return data
        return None


class TaskCodeParser:
    """
    任务码解析器
    比赛规则：四组三位数以"+"连接
    第1组: 第一批3个物料颜色和搬运顺序
    第2组: 第一批物料在粗加工区/暂存区的放置环号
    第3组: 第二批3个物料颜色和搬运顺序
    第4组: 第二批物料在粗加工区的放置环号
    例: 156+123+516+231
    """
    @staticmethod
    def parse(qr_data):
        """
        解析任务码
        :param qr_data: 二维码字符串
        :return: (batch1_colors, batch1_positions, batch2_colors, batch2_positions) 解析失败返回None
        """
        if not qr_data or '+' not in qr_data:
            return None
        try:
            parts = qr_data.split('+')
            if len(parts) != 4:
                return None
            # 每组必须3位数字
            for p in parts:
                if len(p) != 3 or not p.isdigit():
                    return None

            batch1_colors = [int(parts[0][0]), int(parts[0][1]), int(parts[0][2])]
            batch1_positions = [int(parts[1][0]), int(parts[1][1]), int(parts[1][2])]
            batch2_colors = [int(parts[2][0]), int(parts[2][1]), int(parts[2][2])]
            batch2_positions = [int(parts[3][0]), int(parts[3][1]), int(parts[3][2])]

            # 颜色编号必须1~6
            for c in batch1_colors + batch2_colors:
                if c < 1 or c > 6:
                    return None
            # 环号必须1~6
            for r in batch1_positions + batch2_positions:
                if r < 1 or r > 6:
                    return None

            return batch1_colors, batch1_positions, batch2_colors, batch2_positions
        except Exception:
            return None
