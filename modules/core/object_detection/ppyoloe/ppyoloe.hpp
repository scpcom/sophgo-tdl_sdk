#pragma once
#include <bitset>
#include "core.hpp"
#include "core/object/cvai_object_types.h"

namespace cviai {

class PPYoloE final : public Core {
 public:
  PPYoloE();
  virtual ~PPYoloE();
  int inference(VIDEO_FRAME_INFO_S *srcFrame, cvai_object_t *obj_meta);
  virtual bool allowExportChannelAttribute() const override { return true; }
  void set_param(YoloPreParam *p_preprocess_cfg, YoloAlgParam *p_alg_param);

 private:
  virtual int onModelOpened() override;
  virtual int setupInputPreprocess(std::vector<InputPreprecessSetup> *data) override;
  void outputParser(const int image_width, const int image_height, const int frame_width,
                    const int frame_height, cvai_object_t *obj_meta);
  void generate_ppyoloe_proposals(Detections &detections, int frame_width, int frame_height);

  YoloPreParam *p_preprocess_cfg_;
  YoloAlgParam *p_alg_param_;

  std::vector<int> strides_;
  std::map<int, std::string> box_out_names_;
  std::map<int, std::string> cls_out_names_;
};
}  // namespace cviai