#include "image_preprocessor.h"

cv::Mat ImagePreprocessor::process(const cv::Mat& img, Strategy strategy) {
    switch (strategy) {
        case Strategy::NONE:
            return img.clone();
        case Strategy::GRAYSCALE:
            return toGrayscale(img);
        case Strategy::BINARY:
            return toBinary(img);
        case Strategy::ADAPTIVE_BINARY:
            return toAdaptiveBinary(img);
        case Strategy::DENOISE:
            return denoise(img);
        case Strategy::ENHANCE_CONTRAST:
            return enhanceContrast(img);
        case Strategy::AUTO:
        default:
            return autoProcess(img);
    }
}

cv::Mat ImagePreprocessor::toGrayscale(const cv::Mat& img) {
    if (img.channels() == 1) {
        return img.clone();
    }
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat ImagePreprocessor::toBinary(const cv::Mat& img, int threshold) {
    cv::Mat gray = toGrayscale(img);
    cv::Mat binary;
    cv::threshold(gray, binary, threshold, 255, cv::THRESH_BINARY);
    return binary;
}

cv::Mat ImagePreprocessor::toAdaptiveBinary(const cv::Mat& img, int block_size, int C) {
    cv::Mat gray = toGrayscale(img);
    cv::Mat binary;

    // 确保 block_size 是奇数
    if (block_size % 2 == 0) {
        block_size++;
    }

    cv::adaptiveThreshold(gray, binary, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                         cv::THRESH_BINARY, block_size, C);
    return binary;
}

cv::Mat ImagePreprocessor::denoise(const cv::Mat& img, int kernel_size) {
    cv::Mat denoised;
    cv::medianBlur(img, denoised, kernel_size);
    return denoised;
}

cv::Mat ImagePreprocessor::enhanceContrast(const cv::Mat& img) {
    cv::Mat gray = toGrayscale(img);
    cv::Mat enhanced;
    cv::equalizeHist(gray, enhanced);
    return enhanced;
}

cv::Mat ImagePreprocessor::autoProcess(const cv::Mat& img) {
    // 自动预处理流程：
    // 1. 灰度化
    cv::Mat gray = toGrayscale(img);

    // 2. 去噪（轻微）
    cv::Mat denoised;
    cv::medianBlur(gray, denoised, 3);

    // 3. 增强对比度
    cv::Mat enhanced;
    cv::equalizeHist(denoised, enhanced);

    // 4. 自适应二值化（对数字识别效果好）
    cv::Mat binary;
    cv::adaptiveThreshold(enhanced, binary, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                         cv::THRESH_BINARY, 11, 2);

    return binary;
}
