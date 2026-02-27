#pragma once

// 图像识别区域配置
// base_w/base_h 为参考分辨率，运行时会按实际图像尺寸自动缩放
struct ROI {
    int x = 0, y = 0, w = 0, h = 0;
    int base_w = 1280;
    int base_h = 720;
};
