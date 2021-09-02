#include "mask_classification.hpp"

#include "core/cviai_types_mem.h"
#include "rescale_utils.hpp"

#include "core/core/cvai_errno.h"
#include "cvi_sys.h"
#include "opencv2/opencv.hpp"

#define R_SCALE (1 / (256.0 * 0.229))
#define G_SCALE (1 / (256.0 * 0.224))
#define B_SCALE (1 / (256.0 * 0.225))
#define R_MEAN (0.485 / 0.229)
#define G_MEAN (0.456 / 0.224)
#define B_MEAN (0.406 / 0.225)
#define CROP_PCT 0.875
#define MASK_OUT_NAME "logits_dequant"

namespace cviai {

MaskClassification::MaskClassification() : Core(CVI_MEM_DEVICE) {}

MaskClassification::~MaskClassification() {}

int MaskClassification::setupInputPreprocess(std::vector<InputPreprecessSetup> *data) {
  if (data->size() != 1) {
    LOGE("Mask classification only has 1 input.\n");
    return CVIAI_ERR_INVALID_ARGS;
  }
  (*data)[0].factor[0] = R_SCALE;
  (*data)[0].factor[1] = G_SCALE;
  (*data)[0].factor[2] = B_SCALE;
  (*data)[0].mean[0] = R_MEAN;
  (*data)[0].mean[1] = G_MEAN;
  (*data)[0].mean[2] = B_MEAN;
  (*data)[0].use_quantize_scale = true;
  (*data)[0].use_crop = true;

  return CVIAI_SUCCESS;
}

int MaskClassification::inference(VIDEO_FRAME_INFO_S *stOutFrame, cvai_face_t *meta) {
  uint32_t img_width = stOutFrame->stVFrame.u32Width;
  uint32_t img_height = stOutFrame->stVFrame.u32Height;
  for (uint32_t i = 0; i < meta->size; i++) {
    cvai_face_info_t face_info = info_rescale_c(img_width, img_height, *meta, i);
    int box_x1 = face_info.bbox.x1;
    int box_y1 = face_info.bbox.y1;
    uint32_t box_w = face_info.bbox.x2 - face_info.bbox.x1;
    uint32_t box_h = face_info.bbox.y2 - face_info.bbox.y1;
    CVI_AI_FreeCpp(&face_info);
    uint32_t min_edge = std::min(box_w, box_h);
    float new_edge = min_edge * CROP_PCT;
    int box_new_x1 = (box_w - new_edge) / 2.f + box_x1;
    int box_new_y1 = (box_h - new_edge) / 2.f + box_y1;

    m_vpss_config[0].crop_attr.enCropCoordinate = VPSS_CROP_RATIO_COOR;
    m_vpss_config[0].crop_attr.stCropRect = {box_new_x1, box_new_y1, (uint32_t)new_edge,
                                             (uint32_t)new_edge};

    std::vector<VIDEO_FRAME_INFO_S *> frames = {stOutFrame};
    if (int ret = run(frames) != CVIAI_SUCCESS) {
      return ret;
    }

    float *out_data = getOutputRawPtr<float>(MASK_OUT_NAME);

    float max = std::max(out_data[0], out_data[1]);
    float f0 = std::exp(out_data[0] - max);
    float f1 = std::exp(out_data[1] - max);
    float score = f0 / (f0 + f1);

    meta->info[i].mask_score = score;
  }

  return CVIAI_SUCCESS;
}

}  // namespace cviai