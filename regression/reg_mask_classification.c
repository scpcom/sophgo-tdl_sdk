#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cviai.h"
#include "core/utils/vpss_helper.h"

cviai_handle_t facelib_handle = NULL;

static CVI_S32 vpssgrp_width = 1920;
static CVI_S32 vpssgrp_height = 1080;

static int run(const char *img_list, int *mask_count, int *total) {
  FILE *fp;
  if((fp = fopen(img_list, "r")) == NULL) {
    printf("File [%s] open error!\n", img_list);
    return CVI_FAILURE;
  }

  char line[1024];
  while(fscanf(fp, "%[^\n]", line)!=EOF) {
    fgetc(fp);

    printf("%s\n", line);
    VB_BLK blk_fr;
    VIDEO_FRAME_INFO_S frame;
    CVI_S32 ret = CVI_AI_ReadImage(line, &blk_fr, &frame, PIXEL_FORMAT_RGB_888);
    if (ret != CVI_SUCCESS) {
      printf("Read image failed with %#x!\n", ret);
      return ret;
    }

    cvai_face_t face;
    memset(&face, 0, sizeof(cvai_face_t));
    face.size = 1;
    face.width = frame.stVFrame.u32Width;
    face.height = frame.stVFrame.u32Height;
    face.info = (cvai_face_info_t *)malloc(sizeof(cvai_face_info_t) * face.size);
    memset(face.info, 0, sizeof(cvai_face_info_t) * face.size);
    face.info[0].bbox.x1 = 0;
    face.info[0].bbox.y1 = 0;
    face.info[0].bbox.x2 = frame.stVFrame.u32Width;
    face.info[0].bbox.y2 = frame.stVFrame.u32Height;
    face.info[0].face_pts.size = 5;
    face.info[0].face_pts.x = (float *)malloc(sizeof(float) * face.info[0].face_pts.size);
    face.info[0].face_pts.y = (float *)malloc(sizeof(float) * face.info[0].face_pts.size);

    CVI_AI_MaskClassification(facelib_handle, &frame, &face);

    if (face.info[0].mask_score >= 0.5) {
      (*mask_count)++;
    }
    (*total)++;

    CVI_AI_Free(&face);
    CVI_VB_ReleaseBlock(blk_fr);
  }
  fclose(fp);

  return CVI_SUCCESS;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: %s <mask image list> <unmask image list>.\n", argv[0]);
    return CVI_FAILURE;
  }

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

  ret = CVI_AI_SetModelPath(facelib_handle, CVI_AI_SUPPORTED_MODEL_MASKCLASSIFICATION,
                            "/mnt/data/mask_classifier.cvimodel");
  if (ret != CVI_SUCCESS) {
    printf("Set model retinaface failed with %#x!\n", ret);
    return ret;
  }

  int mask = 0, unmask = 0;
  int mask_total = 0, unmask_total = 0;
  run(argv[1], &mask, &mask_total);
  run(argv[2], &unmask, &unmask_total);

  printf("Num of mask face -> tpr: %d/%d, fpr: %d/%d\n", mask, mask_total, unmask, unmask_total);

  CVI_AI_DestroyHandle(facelib_handle);
}
