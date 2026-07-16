#include "obstacle_detector.hpp"

/**
 * @brief 障碍物圆形检测器构造函数
 * @param min_radius 霍夫圆最小检测半径
 * @param max_radius 霍夫圆最大检测半径
 * @param min_area 有效圆形障碍物最小面积阈值
 * 功能：初始化黑色障碍物HSV阈值、形态学核、霍夫圆全套参数
 */
ObstacleDetector::ObstacleDetector(int min_radius, int max_radius, int min_area)
    : _minRadius(min_radius),   // 记录圆最小半径
      _maxRadius(max_radius),   // 记录圆最大半径
      _minArea(min_area)        // 记录圆形最小面积过滤阈值
{
    // HSV 黑色区间下限：H任意0~180，S低饱和度0，V低亮度0
    _lowerBlack = cv::Scalar(0, 0, 0);
    // HSV 黑色区间上限：饱和度≤80，亮度≤60，区分黑与深灰/彩色
    _upperBlack = cv::Scalar(180, 80, 60);

    // 5×5全1卷积核，用于形态学开闭运算，降噪+填充孔洞
    _morphKernel = cv::Mat::ones(5, 5, CV_8UC1);

    // 霍夫梯度圆检测参数初始化
    _houghDp = 1.0;            // 累加器分辨率与原图分辨率比值 1=同分辨率
    _houghMinDist = 50.0;      // 检测到两个圆心之间最小距离，避免重复圆
    _houghParam1 = 50.0;       // Canny边缘检测高阈值
    _houghParam2 = 15.0;       // 累加器阈值，越小检出越多假圆，越大越严格
}

/**
 * @brief 主检测函数：识别画面中黑色圆形障碍物
 * @param img 输入BGR原始图像
 * @return vector<tuple<圆心x,圆心y,半径>> 符合条件的障碍物列表
 */
std::vector<std::tuple<int, int, int>> ObstacleDetector::detect(cv::Mat& img)
{
    // 存储最终有效障碍物（圆心x，圆心y，半径r）
    std::vector<std::tuple<int, int, int>> obstacles;
    // 空图直接返回空结果，防止后续OpenCV接口报错
    if (img.empty())
        return obstacles;

    cv::Mat hsv, mask;
    // 1. BGR原图转HSV色彩空间，HSV对颜色分割更稳定，不受亮度波动影响
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
    // 2. 根据黑色HSV阈值二值化，生成掩膜：黑色区域白色255，其余黑色0
    cv::inRange(hsv, _lowerBlack, _upperBlack, mask);

    // 3. 形态学开运算：先腐蚀后膨胀，消除微小白色噪点、细小杂斑
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, _morphKernel);
    // 4. 形态学闭运算：先膨胀后腐蚀，填充黑色区域内部小孔、连接断裂轮廓
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, _morphKernel);

    cv::Mat circles;
    // 5. 霍夫梯度法检测圆形，仅在黑白掩膜上找圆
    cv::HoughCircles(
        mask,               // 输入二值掩膜
        circles,            // 输出检测圆，每列Vec3f(cx, cy, radius)
        cv::HOUGH_GRADIENT, // 检测算法：梯度法（速度最快）
        _houghDp,
        _houghMinDist,
        _houghParam1,
        _houghParam2,
        _minRadius,         // 过滤过小圆
        _maxRadius          // 过滤过大圆
    );

    // 未检测到任何圆形，直接返回空列表
    if (circles.empty())
        return obstacles;

    // 6. 遍历所有检出圆，按面积二次过滤，剔除过小杂圆
    for (int i = 0; i < circles.cols; i++)
    {
        // 取出单个圆数据：cx, cy, r
        cv::Vec3f c = circles.at<cv::Vec3f>(0, i);
        int cx = static_cast<int>(c[0]);
        int cy = static_cast<int>(c[1]);
        int r = static_cast<int>(c[2]);
        // 圆形面积公式 S=πr²
        double area = CV_PI * r * r;
        // 面积大于最小阈值才判定为有效障碍物
        if (area >= _minArea)
        {
            obstacles.emplace_back(cx, cy, r);
        }
    }
    return obstacles;
}

/**
 * @brief 检测+可视化绘制一体化接口
 * @param img 输入图像，会直接在原图上绘制障碍物标记
 * @return 检测到的障碍物列表，和detect()返回格式一致
 */
std::vector<std::tuple<int, int, int>> ObstacleDetector::detectAndDraw(cv::Mat& img)
{
    // 先调用底层检测逻辑获取障碍物
    std::vector<std::tuple<int, int, int>> obstacles = detect(img);

    // 遍历所有障碍物，在原图绘制标记
    for (const auto& obs : obstacles)
    {
        int cx = std::get<0>(obs);
        int cy = std::get<1>(obs);
        int r = std::get<2>(obs);
        // 红色外圈轮廓：绘制检测到的圆形障碍物边界，线宽2
        cv::circle(img, cv::Point(cx, cy), r, cv::Scalar(0, 0, 255), 2);
        // 绿色实心中心点：标记障碍物圆心，半径2填充圆
        cv::circle(img, cv::Point(cx, cy), 2, cv::Scalar(0, 255, 0), -1);
    }
    return obstacles;
}