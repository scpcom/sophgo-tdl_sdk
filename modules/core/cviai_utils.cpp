#include "core/cviai_utils.h"
#include "cviai_core_internal.hpp"

#include "core/cviai_core.h"
#include "core/cviai_types_mem_internal.h"
#include "core/utils/vpss_helper.h"
#include "utils/core_utils.hpp"

#include <string.h>

int CVI_AI_SQPreprocessRaw(cviai_handle_t handle, const VIDEO_FRAME_INFO_S *frame,
                           VIDEO_FRAME_INFO_S *output, const float quantized_factor,
                           const float quantized_mean, const uint32_t thread) {
  cviai_context_t *ctx = static_cast<cviai_context_t *>(handle);
  uint32_t vpss_thread;
  if (int ret = CVI_AI_AddVpssEngineThread(thread, -1, &vpss_thread, &ctx->vec_vpss_engine) !=
                CVI_SUCCESS) {
    return ret;
  }
  const float factor[] = {quantized_factor, quantized_factor, quantized_factor};
  const float mean[] = {quantized_mean, quantized_mean, quantized_mean};
  VPSS_CHN_ATTR_S chn_attr;
  VPSS_CHN_SQ_HELPER(&chn_attr, frame->stVFrame.u32Width, frame->stVFrame.u32Height,
                     frame->stVFrame.enPixelFormat, factor, mean, false);
  auto &vpss_inst = ctx->vec_vpss_engine[vpss_thread];
  vpss_inst->sendFrame(frame, &chn_attr, 1);
  vpss_inst->getFrame(output, 0);
  return CVI_SUCCESS;
}

int CVI_AI_SQPreprocess(cviai_handle_t handle, const VIDEO_FRAME_INFO_S *frame,
                        VIDEO_FRAME_INFO_S *output, const float factor, const float mean,
                        const float quantize_threshold, const uint32_t thread) {
  float quantized_factor = factor * 128 / quantize_threshold;
  float quantized_mean = (-1) * mean * 128 / quantize_threshold;
  return CVI_AI_SQPreprocessRaw(handle, frame, output, quantized_factor, quantized_mean, thread);
}

int CVI_AI_Dequantize(const int8_t *quantizedData, float *data, const uint32_t bufferSize,
                      const float dequantizeThreshold) {
  cviai::Dequantize(quantizedData, data, dequantizeThreshold, bufferSize);
  return CVI_SUCCESS;
}
int CVI_AI_SoftMax(const float *inputBuffer, float *outputBuffer, const uint32_t bufferSize) {
  cviai::SoftMaxForBuffer(inputBuffer, outputBuffer, bufferSize);
  return CVI_SUCCESS;
}

template <typename FACE>
inline void __attribute__((always_inline)) CVI_AI_InfoCopyToNew(
    const FACE *info, FACE *infoNew,
    typename std::enable_if<std::is_same<FACE, cvai_face_info_t>::value>::type * = 0) {
  CVI_AI_FaceInfoCopyToNew(info, infoNew);
}

template <typename OBJ>
inline void __attribute__((always_inline)) CVI_AI_InfoCopyToNew(
    const OBJ *info, OBJ *infoNew,
    typename std::enable_if<std::is_same<OBJ, cvai_object_info_t>::value>::type * = 0) {
  CVI_AI_ObjInfoCopyToNew(info, infoNew);
}

template <typename T, typename U>
inline int CVI_AI_NMS(const T *input, T *nms, const float threshold, const char method) {
  if (method != 'u' && method != 'm') {
    LOGE("Unsupported NMS method. Only supports u or m");
    return CVI_FAILURE;
  }
  std::vector<U> bboxes;
  std::vector<U> bboxes_nms;
  for (uint32_t i = 0; i < input->size; i++) {
    bboxes.push_back(input->info[i]);
  }
  cviai::NonMaximumSuppression(bboxes, bboxes_nms, threshold, method);
  CVI_AI_Free(nms);
  nms->size = bboxes.size();
  nms->width = input->width;
  nms->height = input->height;
  nms->info = (U *)malloc(nms->size * sizeof(U));
  for (unsigned int i = 0; i < nms->size; i++) {
    CVI_AI_InfoCopyToNew<U>(&bboxes_nms[i], &nms->info[i]);
  }
  return CVI_SUCCESS;
}

int CVI_AI_FaceNMS(const cvai_face_t *face, cvai_face_t *faceNMS, const float threshold,
                   const char method) {
  return CVI_AI_NMS<cvai_face_t, cvai_face_info_t>(face, faceNMS, threshold, method);
}

int CVI_AI_ObjectNMS(const cvai_object_t *obj, cvai_object_t *objNMS, const float threshold,
                     const char method) {
  return CVI_AI_NMS<cvai_object_t, cvai_object_info_t>(obj, objNMS, threshold, method);
}
