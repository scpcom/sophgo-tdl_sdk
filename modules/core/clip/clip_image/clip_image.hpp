#pragma once
#include "core/object/cvtdl_object_types.h"
#include "core_internel.hpp"

namespace cvitdl {

class Clip_Image final : public Core {
 public:
  Clip_Image();
  virtual ~Clip_Image();
  int inference(VIDEO_FRAME_INFO_S *frame, cvtdl_clip_feature *clip_feature);
  virtual bool allowExportChannelAttribute() const override { return true; }

 private:
};
}  // namespace cvitdl