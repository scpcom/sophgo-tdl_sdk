#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "core/cviai_types_mem_internal.h"
#include "core/utils/vpss_helper.h"
#include "cviai.h"
#include "evaluation/cviai_media.h"
#include "sys_utils.hpp"

CVI_S32 get_image_classification(std::string img_path, cviai_handle_t ai_handle,
                                 VIDEO_FRAME_INFO_S* fdFrame, cvai_class_meta_t* meta) {
  CVI_S32 ret = CVI_AI_ReadImage(img_path.c_str(), fdFrame, PIXEL_FORMAT_RGB_888_PLANAR);
  if (ret != CVI_SUCCESS) {
    std::cout << "Convert out video frame failed with :" << ret << ".file:" << img_path
              << std::endl;
    return ret;
  }

  CVI_AI_Image_Classification(ai_handle, fdFrame, meta);

  return ret;
}

void bench_mark_all(std::string bench_path, std::string image_root, std::string res_path,
                    cviai_handle_t ai_handle) {
  std::fstream file(bench_path);
  if (!file.is_open()) {
    printf("can not open bench path %s\n", bench_path.c_str());
    return;
  }
  printf("open bench path %s success!\n", bench_path.c_str());
  std::string line;
  std::stringstream res_ss;
  int cnt = 0;
  while (getline(file, line)) {
    if (!line.empty()) {
      stringstream ss(line);
      std::string image_name;
      while (ss >> image_name) {
        cvai_class_meta_t meta = {0};
        VIDEO_FRAME_INFO_S fdFrame;
        if (++cnt % 100 == 0) {
          cout << "processing idx: " << cnt << endl;
        }
        CVI_S32 ret = get_image_classification(image_root + image_name, ai_handle, &fdFrame, &meta);
        if (ret != CVI_SUCCESS) {
          CVI_AI_Free(&meta);
          CVI_AI_ReleaseImage(&fdFrame);
          break;
        }

        res_ss << image_name << " ";
        for (int i = 0; i < 5; i++) {
          res_ss << " " << meta.cls[i];
        }
        res_ss << "\n";

        CVI_AI_Free(&meta);
        CVI_AI_ReleaseImage(&fdFrame);
        break;
      }
    }
  }

  std::cout << "write results to file: " << res_path << std::endl;
  FILE* fp = fopen(res_path.c_str(), "w");
  fwrite(res_ss.str().c_str(), res_ss.str().size(), 1, fp);
  fclose(fp);
  std::cout << "write done!" << std::endl;
}

int main(int argc, char* argv[]) {
  int vpssgrp_width = 1920;
  int vpssgrp_height = 1080;
  CVI_S32 ret = MMF_INIT_HELPER2(vpssgrp_width, vpssgrp_height, PIXEL_FORMAT_RGB_888, 1,
                                 vpssgrp_width, vpssgrp_height, PIXEL_FORMAT_RGB_888, 1);
  if (ret != CVIAI_SUCCESS) {
    printf("Init sys failed with %#x!\n", ret);
    return ret;
  }

  cviai_handle_t ai_handle = NULL;

  ret = CVI_AI_CreateHandle(&ai_handle);
  if (ret != CVI_SUCCESS) {
    printf("Create ai handle failed with %#x!\n", ret);
    return ret;
  }

  // setup preprocess
  VpssPreParam p_preprocess_cfg;
  float mean[3] = {123.675, 116.28, 103.52};
  float std[3] = {58.395, 57.12, 57.375};

  for (int i = 0; i < 3; i++) {
    p_preprocess_cfg.mean[i] = mean[i] / std[i];
    p_preprocess_cfg.factor[i] = 1.0 / std[i];
  }

  p_preprocess_cfg.format = PIXEL_FORMAT_RGB_888_PLANAR;
  p_preprocess_cfg.use_quantize_scale = true;
  p_preprocess_cfg.rescale_type = RESCALE_CENTER;
  p_preprocess_cfg.keep_aspect_ratio = true;

  printf("setup image classification param \n");
  ret = CVI_AI_Set_Image_Cls_Param(ai_handle, &p_preprocess_cfg);
  printf("image classification set param success!\n");
  if (ret != CVI_SUCCESS) {
    printf("Can not set image classification parameters %#x\n", ret);
    return ret;
  }

  std::string model_path = argv[1];
  std::string bench_path = argv[2];
  std::string image_root = argv[3];
  std::string res_path = argv[4];

  ret =
      CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_IMAGE_CLASSIFICATION, model_path.c_str());
  if (ret != CVI_SUCCESS) {
    printf("open model failed %#x %s!\n", ret, model_path.c_str());
    return ret;
  }
  std::cout << "model opened:" << model_path << std::endl;

  bench_mark_all(bench_path, image_root, res_path, ai_handle);

  CVI_AI_DestroyHandle(ai_handle);

  return ret;
}