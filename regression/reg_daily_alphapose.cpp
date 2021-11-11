#include <fstream>
#include <string>
#include "core/utils/vpss_helper.h"
#include "cviai.h"
#include "evaluation/cviai_evaluation.h"
#include "evaluation/cviai_media.h"
#include "json.hpp"

int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf(
        "Usage: %s <model_dir>\n"
        "          <image_dir>\n"
        "          <regression_json>\n",
        argv[0]);
    return CVIAI_FAILURE;
  }
  // CVI_S32 ret = CVIAI_SUCCESS;
  std::string model_dir = std::string(argv[1]);
  std::string image_dir = std::string(argv[2]);

  nlohmann::json m_json_read;
  std::ofstream m_ofs_results;

  std::ifstream filestr(argv[3]);
  filestr >> m_json_read;
  filestr.close();

  //   "od_model" : "mobiledetv2-pedestrian-d1.cvimodel",
  // std::string od_model_name = std::string(m_json_read["od_model"]);
  // std::string od_model_path = model_dir + "/" + od_model_name;
  // printf("od_model_path: %s\n", od_model_path.c_str());

  std::string pose_model_name = std::string(m_json_read["pose_model"]);
  std::string pose_model_path = model_dir + "/" + pose_model_name;
  // printf("pose_model_path: %s\n", pose_model_path.c_str());

  int img_num = int(m_json_read["test_images"].size());
  // printf("img_num: %d\n", img_num);

  float threshold = float(m_json_read["threshold"]);
  // printf("threshold: %f\n", threshold);

  CVI_S32 ret = CVIAI_SUCCESS;

  // Init VB pool size.
  const CVI_S32 vpssgrp_width = 1920;
  const CVI_S32 vpssgrp_height = 1080;
  ret = MMF_INIT_HELPER2(vpssgrp_width, vpssgrp_height, PIXEL_FORMAT_RGB_888, 5, vpssgrp_width,
                         vpssgrp_height, PIXEL_FORMAT_RGB_888_PLANAR, 5);
  if (ret != CVI_SUCCESS) {
    printf("Init sys failed with %#x!\n", ret);
    return ret;
  }

  // Init cviai handle.
  cviai_handle_t ai_handle = NULL;
  ret = CVI_AI_CreateHandle(&ai_handle);
  if (ret != CVIAI_SUCCESS) {
    printf("Create handle failed with %#x!\n", ret);
    return ret;
  }

  /*
  ret =
      CVI_AI_SetModelPath(ai_handle, CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN,
  od_model_path.c_str()); if (ret != CVIAI_SUCCESS) { printf("Set model retinaface failed with
  %#x!\n", ret); return ret;
  }

  CVI_AI_SetSkipVpssPreprocess(ai_handle, CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN, false);
  CVI_AI_SelectDetectClass(ai_handle, CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN, 1,
                           CVI_AI_DET_TYPE_PERSON);
    */

  ret = CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_ALPHAPOSE, pose_model_path.c_str());
  if (ret != CVIAI_SUCCESS) {
    printf("Set model alphapose failed with %#x!\n", ret);
    return ret;
  }

  for (int img_idx = 0; img_idx < img_num; img_idx++) {
    std::string image_path = image_dir + "/" + std::string(m_json_read["test_images"][img_idx]);
    // printf("[%d] %s\n", img_idx, image_path.c_str());

    int expected_res_num = int(m_json_read["expected_results"][img_idx][0]);
    // printf("expected_res_num %d\n", expected_res_num);

    VB_BLK blk1;
    VIDEO_FRAME_INFO_S frame;

    CVI_S32 ret = CVI_AI_ReadImage(image_path.c_str(), &blk1, &frame, PIXEL_FORMAT_BGR_888);
    if (ret != CVIAI_SUCCESS) {
      printf("Read image failed with %#x!\n", ret);
      return ret;
    }

    cvai_object_t obj;
    memset(&obj, 0, sizeof(cvai_object_t));
    // CVI_AI_MobileDetV2_Pedestrian(ai_handle, &frame, &obj);

    obj.size = expected_res_num;
    obj.height = frame.stVFrame.u32Height;
    obj.width = frame.stVFrame.u32Width;
    obj.info = (cvai_object_info_t *)malloc(obj.size * sizeof(cvai_object_info_t));

    for (uint32_t i = 0; i < obj.size; i++) {
      obj.info[i].bbox.x1 = float(m_json_read["expected_results"][img_idx][1][i][0]);
      obj.info[i].bbox.y1 = float(m_json_read["expected_results"][img_idx][1][i][1]);
      obj.info[i].bbox.x2 = float(m_json_read["expected_results"][img_idx][1][i][2]);
      obj.info[i].bbox.y2 = float(m_json_read["expected_results"][img_idx][1][i][3]);
      obj.info[i].classes = 0;
    }

    /*
    for (uint32_t i = 0; i < obj.size; i++) {
        float x1 = obj.info[i].bbox.x1;
        printf("x1 %f\n",x1);
        float y1 = obj.info[i].bbox.y1;
        printf("y1 %f\n",y1);
        float x2 = obj.info[i].bbox.x2;
        printf("x2 %f\n",x2);
        float y2 = obj.info[i].bbox.y2;
        printf("y2 %f\n",y2);
        int check_class = obj.info[i].classes;
        printf("class %d\n",check_class);
    }
    */

    CVI_AI_AlphaPose(ai_handle, &frame, &obj);

    // print the ground truth
    /*
    for (uint32_t i = 0; i < obj.size; i++) {
      for (int point = 0; point < 17; point++) {
        float point_x = obj.info[i].pedestrian_properity->pose_17.x[point];
        float point_y = obj.info[i].pedestrian_properity->pose_17.y[point];
        printf("[%f, %f], ", point_x, point_y);
      }
      printf("\n");
    }
    */

    for (uint32_t i = 0; i < obj.size; i++) {
      for (int point = 0; point < 17; point++) {
        float point_x = obj.info[i].pedestrian_properity->pose_17.x[point];
        float point_y = obj.info[i].pedestrian_properity->pose_17.y[point];

        float expected_res_x = float(m_json_read["expected_results"][img_idx][2][i][point][0]);
        float expected_res_y = float(m_json_read["expected_results"][img_idx][2][i][point][1]);

        bool pass = (abs(point_x - expected_res_x) < threshold) &
                    (abs(point_y - expected_res_y) < threshold);
        // printf("[%d][%d][%2d] pass: %d; x, y, expected : [%.3f, %.3f], result : [%.3f, %.3f]\n",
        //       img_idx, i, point, pass, expected_res_x, expected_res_y, point_x, point_y);
        if (!pass) {
          return CVIAI_FAILURE;
        }
      }
    }

    // CVI_AI_Free(&obj);
    CVI_VB_ReleaseBlock(blk1);
  }
  CVI_AI_DestroyHandle(ai_handle);
  CVI_SYS_Exit();
  printf("alphapose regression result: all pass\n");
  return CVIAI_SUCCESS;
}