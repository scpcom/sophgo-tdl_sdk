#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cvimath/cvimath.h>

#include "core/utils/vpss_helper.h"
#include "cviai.h"
#include "cviai_perfetto.h"

cviai_handle_t facelib_handle = NULL;

static CVI_S32 vpssgrp_width = 1920;
static CVI_S32 vpssgrp_height = 1080;

int main(int argc, char *argv[]) {
  if (argc != 5) {
    printf("Usage: reg_lfw <retina path> <bmface path> <pair_txt_path> <result file path>.\n");
    printf("Pair txt format: lable image1_path image2_path.\n");
    return CVI_FAILURE;
  }

  CVI_AI_PerfettoInit();
  CVI_S32 ret = CVI_SUCCESS;

  ret = MMF_INIT_HELPER(vpssgrp_width, vpssgrp_height, PIXEL_FORMAT_RGB_888, vpssgrp_width,
                        vpssgrp_height, PIXEL_FORMAT_RGB_888);
  if (ret != CVI_SUCCESS) {
    printf("Init sys failed with %#x!\n", ret);
    return ret;
  }

  ret = CVI_AI_CreateHandle(&facelib_handle);
  if (ret != CVI_SUCCESS) {
    printf("Create handle failed with %#x!\n", ret);
    return ret;
  }

  ret = CVI_AI_SetModelPath(facelib_handle, CVI_AI_SUPPORTED_MODEL_RETINAFACE, argv[1]);
  ret |= CVI_AI_SetModelPath(facelib_handle, CVI_AI_SUPPORTED_MODEL_FACEATTRIBUTE, argv[2]);
  if (ret != CVI_SUCCESS) {
    printf("Set model retinaface failed with %#x!\n", ret);
    return ret;
  }
  CVI_AI_SetModelThreshold(facelib_handle, CVI_AI_SUPPORTED_MODEL_RETINAFACE, 0.8);
  CVI_AI_SetSkipVpssPreprocess(facelib_handle, CVI_AI_SUPPORTED_MODEL_RETINAFACE, false);

  cviai_eval_handle_t eval_handle;
  ret = CVI_AI_Eval_CreateHandle(&eval_handle);
  if (ret != CVI_SUCCESS) {
    printf("Create Eval handle failed with %#x!\n", ret);
    return ret;
  }

  uint32_t imageNum;
  CVI_AI_Eval_LfwInit(eval_handle, argv[3], true, &imageNum);
  for (uint32_t i = 0; i < imageNum; i++) {
    char *name1 = NULL;
    char *name2 = NULL;
    int label;
    CVI_AI_Eval_LfwGetImageLabelPair(eval_handle, i, &name1, &name2, &label);
    VB_BLK blk1;
    VIDEO_FRAME_INFO_S frame1;
    CVI_S32 ret = CVI_AI_ReadImage(name1, &blk1, &frame1, PIXEL_FORMAT_RGB_888);
    if (ret != CVI_SUCCESS) {
      printf("Read image1 failed with %#x!\n", ret);
      return ret;
    }

    VB_BLK blk2;
    VIDEO_FRAME_INFO_S frame2;
    ret = CVI_AI_ReadImage(name2, &blk2, &frame2, PIXEL_FORMAT_RGB_888);
    if (ret != CVI_SUCCESS) {
      printf("Read image2 failed with %#x!\n", ret);
      return ret;
    }
    printf("label %d: image1 %s image2 %s\n", label, name1, name2);
    free(name1);
    free(name2);

    int face_count = 0;
    cvai_face_t face1, face2;
    memset(&face1, 0, sizeof(cvai_face_t));
    memset(&face2, 0, sizeof(cvai_face_t));

    CVI_AI_RetinaFace(facelib_handle, &frame1, &face1, &face_count);
    CVI_AI_RetinaFace(facelib_handle, &frame2, &face2, &face_count);

    CVI_AI_FaceAttribute(facelib_handle, &frame1, &face1);
    CVI_AI_FaceAttribute(facelib_handle, &frame2, &face2);

    CVI_AI_Eval_LfwInsertFace(eval_handle, i, label, &face1, &face2);

    CVI_AI_Free(&face1);
    CVI_AI_Free(&face2);
    CVI_VB_ReleaseBlock(blk1);
    CVI_VB_ReleaseBlock(blk2);
  }

  CVI_AI_Eval_LfwSave2File(eval_handle, argv[4]);
  CVI_AI_Eval_LfwClearInput(eval_handle);
  CVI_AI_Eval_LfwClearEvalData(eval_handle);

  CVI_AI_Eval_DestroyHandle(eval_handle);
  CVI_AI_DestroyHandle(facelib_handle);
  CVI_SYS_Exit();
}
