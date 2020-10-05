#pragma once
#include "core/core/cvai_core_types.h"
#include "cvi_comm_video.h"
#include "cviruntime.h"

#include <algorithm>
#include <vector>

namespace cviai {

void SoftMaxForBuffer(float *src, float *dst, size_t size);
void Dequantize(const int8_t *q_data, float *data, float threshold, size_t size);
void clip_boxes(int width, int height, cvai_bbox_t &box);

enum BOX_RESCALE_TYPE { CENTER, RB };

cvai_bbox_t box_rescale_c(const float frame_width, const float frame_height, const float nn_width,
                          const float nn_height, const cvai_bbox_t bbox, float *ratio,
                          float *pad_width, float *pad_height);
cvai_bbox_t box_rescale_rb(const float frame_width, const float frame_height, const float nn_width,
                           const float nn_height, const cvai_bbox_t bbox, float *ratio);

cvai_bbox_t box_rescale(const float frame_width, const float frame_height, const float nn_width,
                        const float nn_height, const cvai_bbox_t bbox, const BOX_RESCALE_TYPE type);

inline float FastExp(float x) {
  union {
    unsigned int i;
    float f;
  } v;
  v.i = (1 << 23) * (1.4426950409 * x + 126.93490512f);

  return v.f;
}

template <typename T>
void NonMaximumSuppression(std::vector<T> &bboxes, std::vector<T> &bboxes_nms,
                           const float threshold, const char method) {
  std::sort(bboxes.begin(), bboxes.end(),
            [](auto &a, auto &b) { return a.bbox.score > b.bbox.score; });

  int select_idx = 0;
  int num_bbox = bboxes.size();
  std::vector<int> mask_merged(num_bbox, 0);
  bool all_merged = false;

  while (!all_merged) {
    while (select_idx < num_bbox && mask_merged[select_idx] == 1) select_idx++;
    if (select_idx == num_bbox) {
      all_merged = true;
      continue;
    }

    bboxes_nms.emplace_back(bboxes[select_idx]);
    mask_merged[select_idx] = 1;

    cvai_bbox_t select_bbox = bboxes[select_idx].bbox;
    float area1 = static_cast<float>((select_bbox.x2 - select_bbox.x1 + 1) *
                                     (select_bbox.y2 - select_bbox.y1 + 1));
    float x1 = static_cast<float>(select_bbox.x1);
    float y1 = static_cast<float>(select_bbox.y1);
    float x2 = static_cast<float>(select_bbox.x2);
    float y2 = static_cast<float>(select_bbox.y2);

    select_idx++;
    for (int i = select_idx; i < num_bbox; i++) {
      if (mask_merged[i] == 1) continue;

      cvai_bbox_t &bbox_i(bboxes[i].bbox);
      float x = std::max<float>(x1, static_cast<float>(bbox_i.x1));
      float y = std::max<float>(y1, static_cast<float>(bbox_i.y1));
      float w = std::min<float>(x2, static_cast<float>(bbox_i.x2)) - x + 1;
      float h = std::min<float>(y2, static_cast<float>(bbox_i.y2)) - y + 1;
      if (w <= 0 || h <= 0) {
        continue;
      }

      float area2 = static_cast<float>((bbox_i.x2 - bbox_i.x1 + 1) * (bbox_i.y2 - bbox_i.y1 + 1));
      float area_intersect = w * h;
      if (method == 'u' &&
          static_cast<float>(area_intersect) / (area1 + area2 - area_intersect) > threshold) {
        mask_merged[i] = 1;
        continue;
      }
      if (method == 'm' &&
          static_cast<float>(area_intersect) / std::min(area1, area2) > threshold) {
        mask_merged[i] = 1;
      }
    }
  }
}

}  // namespace cviai
