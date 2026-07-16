#include "qr_detector.hpp"

#include <cctype>

/**
 * @brief QR二维码检测器构造函数
 * 使用OpenCV内置QRCodeDetector，无额外参数，默认初始化
 */
QRDetector::QRDetector() = default;

/**
 * @brief 仅检测并解码二维码，不绘制可视化标记
 * @param img 输入BGR图像
 * @return std::optional<std::string> 解码成功返回二维码字符串；失败返回空nullopt
 */
std::optional<std::string> QRDetector::detect(cv::Mat& img)
{
    // 图像为空，直接返回无结果
    if (img.empty())
        return std::nullopt;

    // points：输出二维码四个角点坐标（4个Point2f）
    cv::Mat points;
    // OpenCV内置接口：同时定位二维码 + 解码文本
    std::string data = _decoder.detectAndDecode(img, points);

    // 解码内容为空 = 未识别到有效二维码
    if (data.empty())
        return std::nullopt;

    // 返回二维码原始字符串
    return data;
}

/**
 * @brief 检测二维码 + 在原图绘制边框、中心点标记
 * @param img 输入图像，绘图会直接修改原图
 * @return 解码字符串，识别失败返回nullopt
 */
std::optional<std::string> QRDetector::detectAndDraw(cv::Mat& img)
{
    if (img.empty())
        return std::nullopt;

    cv::Mat points;
    std::string data = _decoder.detectAndDecode(img, points);
    // 两种失败情况：无解码内容 / 没有识别到四个角点
    if (data.empty() || points.empty())
        return std::nullopt;

    // 获取角点总数，标准二维码固定4个顶点
    int n = static_cast<int>(points.total());
    // 绘制蓝色边框：依次连接4个角点，闭合四边形
    for (int j = 0; j < n; j++)
    {
        // 当前顶点
        cv::Point2f p1 = points.at<cv::Point2f>(j);
        // 下一个顶点，最后一点取第0点实现闭合
        cv::Point2f p2 = points.at<cv::Point2f>((j + 1) % n);

        // 浮点坐标转整型像素坐标，绘制蓝色线条，线宽3
        cv::line(img,
                 cv::Point(static_cast<int>(p1.x), static_cast<int>(p1.y)),
                 cv::Point(static_cast<int>(p2.x), static_cast<int>(p2.y)),
                 cv::Scalar(255, 0, 0), 3);
    }

    // 计算二维码几何中心点（四个角点坐标平均值）
    float sumX = 0.0f, sumY = 0.0f;
    for (int i = 0; i < n; i++)
    {
        cv::Point2f p = points.at<cv::Point2f>(i);
        sumX += p.x;
        sumY += p.y;
    }
    // 平均得到中心像素坐标
    int cx = static_cast<int>(sumX / n);
    int cy = static_cast<int>(sumY / n);
    // 绘制绿色实心圆心标记二维码中心
    cv::circle(img, cv::Point(cx, cy), 5, cv::Scalar(0, 255, 0), -1);

    return data;
}

/**
 * @brief QR数据解析器：将二维码字符串解析为任务结构体TaskCode
 * 协议格式规范：AAA+BBB+CCC+DDD
 * 每一段固定3位数字
 * 四段含义：
 *  第一段 batch1_colors 第一组物料颜色编号
 *  第二段 batch1_positions 第一组投放位置编号
 *  第三段 batch2_colors 第二组物料颜色编号
 *  第四段 batch2_positions 第二组投放位置编号
 * @param qr_data 二维码读取到的原始字符串
 * @return 校验通过返回结构化任务数据，格式错误返回nullopt
 */
std::optional<TaskCode> TaskCodeParser::parse(const std::string& qr_data)
{
    // 空字符串直接判定无效
    if (qr_data.empty())
        return std::nullopt;
    // 协议必须包含分隔符'+'，无分隔符直接丢弃
    if (qr_data.find('+') == std::string::npos)
        return std::nullopt;

    // 以 '+' 分割字符串，存入字符串数组parts
    std::vector<std::string> parts;
    std::string current;
    for (char c : qr_data)
    {
        if (c == '+')
        {
            parts.push_back(current);
            current.clear();
        }
        else
        {
            current += c;
        }
    }
    // 循环结束把最后一段加入数组
    parts.push_back(current);

    // 协议强制要求恰好4段，多/少段均为非法二维码
    if (parts.size() != 4)
        return std::nullopt;

    // 逐段校验：每一段必须严格3位、全部是数字
    for (const auto& p : parts)
    {
        // 长度不为3，格式错误
        if (p.length() != 3)
            return std::nullopt;
        // 每一个字符必须是数字0~9
        for (char c : p)
        {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return std::nullopt;
        }
    }

    // 局部lambda工具函数：3位数字字符串转int数组
    auto extractDigits = [](const std::string& s)
    {
        // 字符减去'0'ASCII偏移得到对应数字
        return std::vector<int>{s[0] - '0', s[1] - '0', s[2] - '0'};
    };

    // 填充任务结构体四段数据
    TaskCode code;
    code.batch1_colors = extractDigits(parts[0]);
    code.batch1_positions = extractDigits(parts[1]);
    code.batch2_colors = extractDigits(parts[2]);
    code.batch2_positions = extractDigits(parts[3]);

    // 业务校验1：颜色编号范围 1~6
    for (int c : code.batch1_colors)
    {
        if (c < 1 || c > 6)
            return std::nullopt;
    }
    for (int c : code.batch2_colors)
    {
        if (c < 1 || c > 6)
            return std::nullopt;
    }

    // 业务校验2：投放位置编号范围 1~6
    for (int r : code.batch1_positions)
    {
        if (r < 1 || r > 6)
            return std::nullopt;
    }
    for (int r : code.batch2_positions)
    {
        if (r < 1 || r > 6)
            return std::nullopt;
    }

    // 所有格式、数值校验全部通过，返回任务指令
    return code;
}