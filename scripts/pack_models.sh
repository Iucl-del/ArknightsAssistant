#!/bin/bash
# 将 OCR 模型打包为 models.tar.gz，用于上传到 GitHub Releases
# 用法: bash scripts/pack_models.sh

set -e
cd "$(dirname "$0")/.."

OUTPUT="models.tar.gz"

echo "📦 打包模型文件..."
tar -czf "${OUTPUT}" \
    -C models \
    onnx/ch_ppocr_det.onnx \
    onnx/ch_ppocr_cls.onnx \
    onnx/ch_ppocr_rec.onnx \
    ppocr_keys_v1.txt

SIZE=$(du -sh "${OUTPUT}" | cut -f1)
echo "✅ 打包完成: ${OUTPUT} (${SIZE})"
echo ""
echo "下一步："
echo "  1. 在 GitHub 仓库创建 Release（如 v1.0-models）"
echo "  2. 上传 ${OUTPUT} 到该 Release"
echo "  3. 复制下载链接，在构建时传入："
echo "     cmake --preset linux-x64-vcpkg -DMODELS_DOWNLOAD_URL=<下载链接>"
