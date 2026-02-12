#include "ocr_det.h"
#include <algorithm>

TextDetector::TextDetector(Ort::Env& env, const std::string& model_path)
    : session_(env, model_path.c_str(), Ort::SessionOptions{nullptr}) {

    size_t num_input = session_.GetInputCount();
    size_t num_output = session_.GetOutputCount();

    // 保存节点名称的临时字符串
    input_name_strings_.resize(num_input);
    output_name_strings_.resize(num_output);

    for (size_t i = 0; i < num_input; i++) {
        auto name = session_.GetInputNameAllocated(i, allocator_);
        input_name_strings_[i] = name.get();
        input_names_.push_back(input_name_strings_[i].c_str());
    }
    for (size_t i = 0; i < num_output; i++) {
        auto name = session_.GetOutputNameAllocated(i, allocator_);
        output_name_strings_[i] = name.get();
        output_names_.push_back(output_name_strings_[i].c_str());
    }
}

cv::Mat TextDetector::preprocess(const cv::Mat& img, float& ratio_h, float& ratio_w) {
    int max_side_len = 960;
    int h = img.rows;
    int w = img.cols;

    float ratio = 1.0f;
    if (std::max(h, w) > max_side_len) {
        ratio = max_side_len * 1.0f / std::max(h, w);
    }

    int resize_h = int(h * ratio);
    int resize_w = int(w * ratio);

    resize_h = (resize_h + 31) / 32 * 32;
    resize_w = (resize_w + 31) / 32 * 32;

    cv::Mat resized;
    cv::resize(img, resized, cv::Size(resize_w, resize_h));

    ratio_h = resize_h * 1.0f / h;
    ratio_w = resize_w * 1.0f / w;

    resized.convertTo(resized, CV_32FC3, 1.0 / 255.0);

    cv::Mat normalized;
    cv::subtract(resized, cv::Scalar(0.485, 0.456, 0.406), normalized);
    cv::divide(normalized, cv::Scalar(0.229, 0.224, 0.225), normalized);

    return normalized;
}

std::vector<TextBox> TextDetector::postprocess(const cv::Mat& pred, float ratio_h, float ratio_w) {
    std::vector<TextBox> boxes;

    cv::Mat mask;
    cv::threshold(pred, mask, 0.3, 255, cv::THRESH_BINARY);
    mask.convertTo(mask, CV_8UC1);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        if (contour.size() <= 2) continue;

        cv::RotatedRect box = cv::minAreaRect(contour);
        if (std::min(box.size.width, box.size.height) < 3) continue;

        // 计算边界框的得分（基于原始预测图）
        float box_score = 0.0f;
        int count = 0;
        for (const auto& pt : contour) {
            if (pt.x >= 0 && pt.x < pred.cols && pt.y >= 0 && pt.y < pred.rows) {
                box_score += pred.at<float>(pt.y, pt.x);
                count++;
            }
        }
        if (count > 0) {
            box_score /= count;
        }

        cv::Point2f vertices[4];
        box.points(vertices);

        // 先映射回原图坐标
        std::vector<cv::Point2f> mapped_pts;
        for (int i = 0; i < 4; i++) {
            cv::Point2f pt;
            pt.x = vertices[i].x / ratio_w;
            pt.y = vertices[i].y / ratio_h;
            mapped_pts.push_back(pt);
        }

        // 计算中心点
        cv::Point2f center(0, 0);
        for (const auto& pt : mapped_pts) {
            center.x += pt.x;
            center.y += pt.y;
        }
        center.x /= 4.0f;
        center.y /= 4.0f;

        // 从中心向外扩展20%，确保完全覆盖文字
        TextBox text_box;
        float expand_ratio = 1.7f;  // 扩展20%
        for (const auto& pt : mapped_pts) {
            cv::Point2f expanded_pt;
            expanded_pt.x = center.x + (pt.x - center.x) * expand_ratio;
            expanded_pt.y = center.y + (pt.y - center.y) * expand_ratio;
            text_box.box.push_back(expanded_pt);
        }

        text_box.score = box_score;
        boxes.push_back(text_box);
    }

    return boxes;
}

std::vector<TextBox> TextDetector::detect(const cv::Mat& img) {
    float ratio_h, ratio_w;
    cv::Mat input = preprocess(img, ratio_h, ratio_w);

    std::vector<int64_t> input_shape = {1, 3, input.rows, input.cols};
    size_t input_tensor_size = 1 * 3 * input.rows * input.cols;
    std::vector<float> input_tensor_values(input_tensor_size);

    std::vector<cv::Mat> channels(3);
    cv::split(input, channels);
    for (int c = 0; c < 3; c++) {
        std::memcpy(input_tensor_values.data() + c * input.rows * input.cols,
                   channels[c].data, input.rows * input.cols * sizeof(float));
    }

    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_tensor_values.data(), input_tensor_size,
        input_shape.data(), input_shape.size());

    auto output_tensors = session_.Run(Ort::RunOptions{nullptr},
                                       input_names_.data(), &input_tensor, 1,
                                       output_names_.data(), 1);

    float* output = output_tensors[0].GetTensorMutableData<float>();
    auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();

    int out_h = output_shape[2];
    int out_w = output_shape[3];
    cv::Mat pred(out_h, out_w, CV_32FC1, output);

    // 计算从输出特征图到预处理后图像的比例
    float ratio_h_out = static_cast<float>(input.rows) / out_h;
    float ratio_w_out = static_cast<float>(input.cols) / out_w;

    return postprocess(pred, ratio_h * ratio_h_out, ratio_w * ratio_w_out);
}

cv::Mat TextDetector::getRotateCropImage(const cv::Mat& img, const std::vector<cv::Point2f>& box) {
    float left = box[0].x, right = box[0].x, top = box[0].y, bottom = box[0].y;
    for (const auto& pt : box) {
        left = std::min(left, pt.x);
        right = std::max(right, pt.x);
        top = std::min(top, pt.y);
        bottom = std::max(bottom, pt.y);
    }

    cv::Mat crop = img(cv::Rect(left, top, right - left, bottom - top));
    return crop;
}
