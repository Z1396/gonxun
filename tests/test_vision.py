"""
vision 子包单元测试
验证新规则实现的正确性
"""
import sys
import os

# 添加项目根目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import cv2
import numpy as np

from vision import (
    ColorDetector, ThreeRingDetector, SixRingDetector,
    QRDetector, TaskCodeParser, KalmanFilter,
    SerialComm, CameraManager, TaskDisplay, ObstacleDetector,
    COLOR_DIST, COLOR_ID_MAP, RING_SCORES,
    calc_placement_score
)


def run_all_tests():
    """运行全部单元测试"""
    print("=" * 60)
    print("运行单元测试 - vision 子包 v3.0")
    print("=" * 60)

    passed = 0
    failed = 0

    # ==================== color_detector 测试 ====================
    print("\n[color_detector] 6种颜色HSV阈值")
    expected = {'red', 'yellow', 'blue', 'green', 'black', 'light_blue'}
    if set(COLOR_DIST.keys()) == expected:
        print("  [PASS] 6种颜色全部存在")
        passed += 1
    else:
        print("  [FAIL] 颜色缺失")
        failed += 1

    print("\n[color_detector] 颜色编号映射")
    expected_map = {1: 'red', 2: 'yellow', 3: 'blue', 4: 'green', 5: 'black', 6: 'light_blue'}
    actual_map = {k: v[0] for k, v in COLOR_ID_MAP.items()}
    if actual_map == expected_map:
        print("  [PASS] 颜色编号与比赛规则一致")
        passed += 1
    else:
        print("  [FAIL] 编号映射错误")
        failed += 1

    print("\n[color_detector] ColorDetector实例化")
    try:
        detector = ColorDetector()
        if detector.color_dist == COLOR_DIST:
            print("  [PASS] ColorDetector实例化并加载阈值")
            passed += 1
        else:
            print("  [FAIL] 阈值加载错误")
            failed += 1
    except Exception as e:
        print(f"  [FAIL] {e}")
        failed += 1

    # ==================== ring_detector 测试 ====================
    print("\n[ring_detector] 6环尺寸评分表")
    expected_scores = {1: 15, 2: 10, 3: 7, 4: 5, 5: 3, 6: 1}
    if RING_SCORES == expected_scores:
        print("  [PASS] 评分与比赛规则表3一致")
        passed += 1
    else:
        print("  [FAIL] 评分错误")
        failed += 1

    print("\n[ring_detector] calc_placement_score得分计算")
    test_cases = [(1, False, 15), (2, False, 10), (3, False, 7),
                  (4, False, 5), (5, False, 3), (6, False, 1),
                  (None, False, 0), (0, False, 0), (1, True, 0)]
    all_ok = True
    for ring, fallen, expected_score in test_cases:
        score = calc_placement_score(ring, fallen)
        if score != expected_score:
            print(f"  [FAIL] 环{ring} 倾倒{fallen} 期望{expected_score} 实际{score}")
            all_ok = False
    if all_ok:
        print("  [PASS] 全部得分场景正确")
        passed += 1
    else:
        failed += 1

    print("\n[ring_detector] SixRingDetector检测6个圆环")
    test_img = np.ones((400, 800, 3), dtype=np.uint8) * 255
    for i in range(6):
        cv2.circle(test_img, (80 + i * 130, 200), 40, (0, 0, 0), 2)
    six_detector = SixRingDetector()
    rings = six_detector.detect(test_img)
    if rings is not None and len(rings) == 6:
        print(f"  [PASS] 成功检测到6个圆环 {list(rings.keys())}")
        passed += 1
    else:
        print(f"  [FAIL] 仅检测到{rings}")
        failed += 1

    print("\n[ring_detector] ThreeRingDetector实例化")
    try:
        three_detector = ThreeRingDetector()
        if three_detector is not None:
            print("  [PASS] ThreeRingDetector实例化成功")
            passed += 1
    except Exception as e:
        print(f"  [FAIL] {e}")
        failed += 1

    # ==================== qr_detector 测试 ====================
    print("\n[qr_detector] TaskCodeParser 标准格式解析")
    parser = TaskCodeParser()
    result = parser.parse("156+123+516+231")
    expected = ([1, 5, 6], [1, 2, 3], [5, 1, 6], [2, 3, 1])
    if result == expected:
        print("  [PASS] 解析结果正确")
        passed += 1
    else:
        print(f"  [FAIL] 期望{expected}，实际{result}")
        failed += 1

    print("\n[qr_detector] TaskCodeParser 错误格式拒绝")
    bad_ok = True
    for bad in ["123+456+789", "1a2+123+123+123", "123+123+123", "", None]:
        if parser.parse(bad) is not None:
            print(f"  [FAIL] '{bad}' 未被拒绝")
            bad_ok = False
    if bad_ok:
        print("  [PASS] 全部错误格式被正确拒绝")
        passed += 1
    else:
        failed += 1

    print("\n[qr_detector] 颜色超界拒绝")
    if parser.parse("789+123+123+123") is None:
        print("  [PASS] 颜色超界被拒绝")
        passed += 1
    else:
        print("  [FAIL] 颜色超界未被拒绝")
        failed += 1

    print("\n[qr_detector] QRDetector实例化")
    try:
        qr = QRDetector()
        if qr.decoder is not None:
            print("  [PASS] QRDetector实例化成功")
            passed += 1
    except Exception as e:
        print(f"  [FAIL] {e}")
        failed += 1

    # ==================== kalman_filter 测试 ====================
    print("\n[kalman_filter] 滤波器初始化和滤波")
    kf = KalmanFilter()
    z1 = np.array([[100], [200]], dtype=np.float32)
    z2 = np.array([[105], [203]], dtype=np.float32)
    r1 = kf.filter(z1)
    r2 = kf.filter(z2)
    if kf.initialized and r1.shape == (2, 1) and r2.shape == (2, 1):
        print(f"  [PASS] 滤波成功 r1=({r1[0][0]:.1f},{r1[1][0]:.1f}) r2=({r2[0][0]:.1f},{r2[1][0]:.1f})")
        passed += 1
    else:
        print("  [FAIL] 滤波失败")
        failed += 1

    print("\n[kalman_filter] 兼容元组输入")
    kf.reset()
    r = kf.filter((50, 60))
    if r.shape == (2, 1):
        print("  [PASS] 元组输入支持")
        passed += 1
    else:
        print("  [FAIL] 元组输入失败")
        failed += 1

    # ==================== serial_comm 测试 ====================
    print("\n[serial_comm] SerialComm模拟模式启动")
    try:
        sc = SerialComm(mock=True)
        sc.start()
        sc.close()
        print("  [PASS] 模拟模式启动/停止成功")
        passed += 1
    except Exception as e:
        print(f"  [FAIL] {e}")
        failed += 1

    print("\n[serial_comm] 协议常量正确")
    from vision.serial_comm import FRAME_HEADER, FRAME_TAIL
    if FRAME_HEADER == 0x66 and FRAME_TAIL == 0x77:
        print("  [PASS] 帧头0x66 帧尾0x77 正确")
        passed += 1
    else:
        print("  [FAIL] 协议常量错误")
        failed += 1

    # ==================== camera_manager 测试 ====================
    print("\n[camera_manager] CameraManager实例化")
    try:
        cm = CameraManager(main_index=0, qr_index=2)
        if cm.main_index == 0 and cm.qr_index == 2:
            print("  [PASS] CameraManager实例化 (main=0, qr=2)")
            passed += 1
    except Exception as e:
        print(f"  [FAIL] {e}")
        failed += 1

    # ==================== task_display 测试 ====================
    print("\n[task_display] 任务码显示图生成")
    td = TaskDisplay(width=400, height=200)
    display = td.render("156+123+516+231", completed_steps=[True, True, False, False])
    if display is not None and display.shape == (200, 400, 3):
        print("  [PASS] 显示图生成成功 shape=(200, 400, 3)")
        passed += 1
    else:
        print(f"  [FAIL] 形状错误 {display.shape if display is not None else None}")
        failed += 1

    # ==================== obstacle_detector 测试 ====================
    print("\n[obstacle_detector] 黑色障碍物检测")
    od = ObstacleDetector()
    test_obs_img = np.ones((400, 600, 3), dtype=np.uint8) * 200
    cv2.circle(test_obs_img, (150, 200), 25, (0, 0, 0), -1)
    cv2.circle(test_obs_img, (450, 200), 25, (0, 0, 0), -1)
    obstacles = od.detect(test_obs_img)
    if len(obstacles) >= 1:
        print(f"  [PASS] 检测到{len(obstacles)}个障碍物")
        passed += 1
    else:
        print("  [FAIL] 未检测到障碍物")
        failed += 1

    # ==================== 汇总 ====================
    print("\n" + "=" * 60)
    print(f"测试结果: 通过{passed} / 失败{failed} / 总计{passed+failed}")
    print("=" * 60)
    return failed == 0


if __name__ == "__main__":
    success = run_all_tests()
    sys.exit(0 if success else 1)
