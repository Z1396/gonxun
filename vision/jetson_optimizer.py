"""
Jetson Nano 性能优化模块
功能：
- 监控 GPU/CPU/内存使用率
- 自动调整推理参数避免 OOM
- 提供性能调优建议
"""
import os
import logging
import subprocess

logger = logging.getLogger(__name__)


def is_jetson():
    """检测是否在 Jetson 平台上运行"""
    try:
        with open('/etc/nv_tegra_release', 'r') as f:
            return True
    except FileNotFoundError:
        return False
    except Exception:
        return False


def get_gpu_memory_info():
    """获取 GPU 内存信息（Jetson 共享内存）"""
    if not is_jetson():
        return None

    try:
        # Jetson 使用 tegrastats 获取信息
        result = subprocess.run(
            ['tegrastats', '--interval', '1'],
            capture_output=True, text=True, timeout=2
        )

        # 解析输出
        # 示例：RAM 2048/3964MB (lfb 2x4MB) CPU [1%@102,off,off,off]...
        output = result.stdout.strip()

        # 提取内存信息
        if 'RAM' in output:
            parts = output.split('RAM')[1].split('MB')[0]
            used, total = parts.strip().split('/')
            return {
                'used_mb': int(used),
                'total_mb': int(total),
                'free_mb': int(total) - int(used),
                'usage_percent': int(used) / int(total) * 100
            }
    except Exception as e:
        logger.debug(f"获取内存信息失败: {e}")

    return None


def get_gpu_frequency():
    """获取 GPU 当前频率"""
    if not is_jetson():
        return None

    try:
        # 读取 GPU 频率
        freq_path = '/sys/devices/17000000.gp10b/devfreq/17000000.gp10b/cur_freq'
        if os.path.exists(freq_path):
            with open(freq_path, 'r') as f:
                freq_hz = int(f.read().strip())
                return freq_hz / 1e6  # 转换为 MHz
    except Exception:
        pass

    return None


def optimize_for_jetson():
    """
    Jetson Nano 性能优化建议
    返回优化后的参数字典
    """
    if not is_jetson():
        logger.info("非 Jetson 平台，使用默认参数")
        return {
            'imgsz': 640,
            'batch_size': 8,
            'workers': 0,
            'half': False
        }

    # 获取内存信息
    mem_info = get_gpu_memory_info()

    # 根据内存情况调整参数
    if mem_info and mem_info['total_mb'] <= 4096:
        # Jetson Nano 4GB 版本
        logger.info("[Jetson Nano 4GB] 内存优化模式")

        # 检查当前内存使用率
        if mem_info['usage_percent'] > 80:
            logger.warning(f"内存使用率过高: {mem_info['usage_percent']:.1f}%")

        return {
            'imgsz': 320,      # 降低推理尺寸
            'batch_size': 1,   # 最小批次
            'workers': 0,      # 禁用多进程
            'half': True       # FP16 半精度
        }
    else:
        # Jetson Nano 2GB 版本或其他
        logger.info("[Jetson] 标准优化模式")
        return {
            'imgsz': 320,
            'batch_size': 1,
            'workers': 0,
            'half': True
        }


def enable_performance_mode():
    """开启 Jetson 最大性能模式"""
    if not is_jetson():
        logger.warning("非 Jetson 平台，无法开启性能模式")
        return False

    try:
        # 开启最大频率
        subprocess.run(['sudo', 'jetson_clocks'], check=True)
        logger.info("[Jetson] 已开启最大性能模式")
        return True
    except subprocess.CalledProcessError as e:
        logger.error(f"开启性能模式失败: {e}")
        return False
    except FileNotFoundError:
        logger.warning("jetson_clocks 命令不存在")
        return False


def disable_performance_mode():
    """关闭 Jetson 性能模式（恢复默认）"""
    if not is_jetson():
        return False

    try:
        subprocess.run(['sudo', 'jetson_clocks', '--restore'], check=True)
        logger.info("[Jetson] 已恢复默认性能模式")
        return True
    except Exception as e:
        logger.error(f"恢复性能模式失败: {e}")
        return False


def print_system_status():
    """打印系统状态信息"""
    if not is_jetson():
        print("非 Jetson 平台")
        return

    print("\n" + "=" * 50)
    print("Jetson 系统状态")
    print("=" * 50)

    # 内存信息
    mem_info = get_gpu_memory_info()
    if mem_info:
        print(f"\n内存使用:")
        print(f"  已用: {mem_info['used_mb']} MB")
        print(f"  总量: {mem_info['total_mb']} MB")
        print(f"  空闲: {mem_info['free_mb']} MB")
        print(f"  使用率: {mem_info['usage_percent']:.1f}%")

    # GPU 频率
    gpu_freq = get_gpu_frequency()
    if gpu_freq:
        print(f"\nGPU 频率: {gpu_freq:.0f} MHz")

    # CPU 温度（Jetson）
    try:
        temp_path = '/sys/devices/virtual/thermal/thermal_zone0/temp'
        if os.path.exists(temp_path):
            with open(temp_path, 'r') as f:
                temp = int(f.read().strip()) / 1000
                print(f"CPU 温度: {temp:.1f}°C")
    except Exception:
        pass

    print("\n" + "=" * 50)


class JetsonPerformanceMonitor:
    """Jetson 性能监控器"""

    def __init__(self, interval=5):
        """
        :param interval: 监控间隔（秒）
        """
        self.interval = interval
        self._running = False

    def start(self):
        """启动监控（后台线程）"""
        if not is_jetson():
            logger.warning("非 Jetson 平台，监控不可用")
            return

        import threading
        self._running = True
        self._thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self._thread.start()
        logger.info(f"性能监控已启动（间隔 {self.interval}s）")

    def stop(self):
        """停止监控"""
        self._running = False
        logger.info("性能监控已停止")

    def _monitor_loop(self):
        """监控循环"""
        import time
        while self._running:
            mem_info = get_gpu_memory_info()
            if mem_info and mem_info['usage_percent'] > 90:
                logger.warning(f"内存使用率过高: {mem_info['usage_percent']:.1f}%")
                logger.warning("建议：降低 batch_size 或 imgsz")

            time.sleep(self.interval)


# 使用示例
if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)

    print_system_status()

    print("\n优化参数:")
    params = optimize_for_jetson()
    for key, value in params.items():
        print(f"  {key}: {value}")