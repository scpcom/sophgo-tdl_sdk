#ifndef _CVI_FACE_UTILS_H_
#define _CVI_FACE_UTILS_H_

#include "core/face/cvai_face_types.h"
#include "opencv2/core.hpp"

#include "cvi_comm.h"

namespace cviai {
int face_align(const cv::Mat &image, cv::Mat &aligned, const cvai_face_info_t &face_info);
int face_align_gdc(const VIDEO_FRAME_INFO_S *inFrame, VIDEO_FRAME_INFO_S *outFrame,
                   const cvai_face_info_t &face_info);
}  // namespace cviai
#endif
