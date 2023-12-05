#pragma once
#include "core/face/cvtdl_face_types.h"
#include "core_internel.hpp"

namespace cvitdl {

class ThermalFace final : public Core {
 public:
  ThermalFace();
  virtual ~ThermalFace();
  int inference(VIDEO_FRAME_INFO_S *srcFrame, cvtdl_face_t *meta);
  virtual bool allowExportChannelAttribute() const override { return true; }

 private:
  virtual int setupInputPreprocess(std::vector<InputPreprecessSetup> *data) override;
  virtual int onModelOpened() override;
  virtual int vpssPreprocess(VIDEO_FRAME_INFO_S *srcFrame, VIDEO_FRAME_INFO_S *dstFrame,
                             VPSSConfig &vpss_config) override;
  void outputParser(const int image_width, const int image_height, const int frame_width,
                    const int frame_height, cvtdl_face_t *meta);

  std::vector<cvtdl_bbox_t> m_all_anchors;
};
}  // namespace cvitdl