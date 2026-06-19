"""
二维码识别与任务码解析模块
- 二维码识别
- 任务码格式校验与解析 (XXX+XXX+XXX+XXX)
"""
import cv2


class QRDetector:
    """二维码识别器"""

    _BBOX_COLOR = (255, 0, 0)
    _BBOX_THICKNESS = 3
    _CENTER_COLOR = (0, 255, 0)
    _CENTER_RADIUS = 5

    def __init__(self):
        self.decoder = cv2.QRCodeDetector()

    def _decode(self, img):
        """统一解码接口，返回 (data, bbox)"""
        data, bbox, _ = self.decoder.detectAndDecode(img)
        return data, bbox

    def detect(self, img):
        """识别二维码，返回字符串；无二维码返回None"""
        if img is None:
            return None
        data, _ = self._decode(img)
        return data if data else None

    def detect_and_draw(self, img):
        """识别二维码并绘制标记"""
        if img is None:
            return None
        data, bbox = self._decode(img)
        if not data or bbox is None:
            return None

        n = len(bbox[0])
        for j in range(n):
            p1 = tuple(bbox[0][j])
            p2 = tuple(bbox[0][(j + 1) % n])
            cv2.line(img, p1, p2, self._BBOX_COLOR, self._BBOX_THICKNESS)

        xs = [p[0] for p in bbox[0]]
        ys = [p[1] for p in bbox[0]]
        cx = int(sum(xs) / n)
        cy = int(sum(ys) / n)
        cv2.circle(img, (cx, cy), self._CENTER_RADIUS, self._CENTER_COLOR, -1)
        return data


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

        :return: (batch1_colors, batch1_positions, batch2_colors, batch2_positions)
                 解析失败返回None
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

            # 颜色编号和环号必须1~6
            for c in batch1_colors + batch2_colors:
                if c < 1 or c > 6:
                    return None
            for r in batch1_positions + batch2_positions:
                if r < 1 or r > 6:
                    return None

            return batch1_colors, batch1_positions, batch2_colors, batch2_positions
        except Exception:
            return None
