/**
 * @file field_constants.hpp
 * @brief 场地物理常量定义。
 *
 * 定义工创赛智能物流搬运系统的场地尺寸等物理常量，
 * 供路径规划、运动控制等模块统一引用。
 */

#pragma once

namespace gonxun {

/// 场地边长，单位 mm。赛场为 2400×2400mm 正方形区域
constexpr int FIELD_SIZE_MM = 2400;

} // namespace gonxun
