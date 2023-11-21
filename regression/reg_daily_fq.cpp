#include <experimental/filesystem>
#include <fstream>
#include <memory>
#include <string>
#include "core/utils/vpss_helper.h"
#include "cvi_tdl.h"
#include "cvi_tdl_evaluation.h"
#include "cvi_tdl_media.h"
#include "cvi_tdl_test.hpp"
#include "gtest.h"
#include "json.hpp"
#include "raii.hpp"
#include "regression_utils.hpp"

namespace fs = std::experimental::filesystem;
namespace cvitdl {
namespace unitest {

class FaceQualityTestSuite : public CVI_TDLModelTestSuite {
 public:
  FaceQualityTestSuite() : CVI_TDLModelTestSuite("daily_reg_FQ.json", "reg_daily_fq") {}

  virtual ~FaceQualityTestSuite() = default;

  std::string m_model_path;

 protected:
  virtual void SetUp() {
    std::string model_name = std::string(m_json_object["reg_config"][0]["model_name"]);
    m_model_path = (m_model_dir / fs::path(model_name)).string();

    m_tdl_handle = NULL;
    ASSERT_EQ(CVI_TDL_CreateHandle2(&m_tdl_handle, 1, 0), CVI_TDL_SUCCESS);
    ASSERT_EQ(CVI_TDL_SetVpssTimeout(m_tdl_handle, 1000), CVI_TDL_SUCCESS);
  }

  virtual void TearDown() {
    CVI_TDL_DestroyHandle(m_tdl_handle);
    m_tdl_handle = NULL;
  }
};

TEST_F(FaceQualityTestSuite, open_close_model) {
  TDLModelHandler tdlmodel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_FACEQUALITY, m_model_path.c_str(),
                           false);
  ASSERT_NO_FATAL_FAILURE(tdlmodel.open());

  const char *model_path_get =
      CVI_TDL_GetModelPath(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_FACEQUALITY);

  EXPECT_PRED2([](auto s1, auto s2) { return s1 == s2; }, m_model_path,
               std::string(model_path_get));
}

TEST_F(FaceQualityTestSuite, get_vpss_config) {
  TDLModelHandler tdlmodel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_FACEQUALITY, m_model_path.c_str(),
                           false);
  ASSERT_NO_FATAL_FAILURE(tdlmodel.open());
  cvtdl_vpssconfig_t vpssconfig;
  vpssconfig.chn_attr.u32Height = 200;
  vpssconfig.chn_attr.u32Width = 200;
  vpssconfig.chn_attr.enPixelFormat = PIXEL_FORMAT_ARGB_1555;
  vpssconfig.chn_attr.stNormalize.bEnable = false;

  // CVI_TDL_GetVpssChnConfig for CVI_TDL_SUPPORTED_MODEL_FACEQUALITY should be failed.
  EXPECT_EQ(CVI_TDL_GetVpssChnConfig(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_FACEQUALITY, 100, 100, 0,
                                     &vpssconfig),
            CVI_TDL_ERR_GET_VPSS_CHN_CONFIG);

  // make sure doesn't modify vpss config after CVI_TDL_GetVpssChnConfig is called.
  EXPECT_EQ(vpssconfig.chn_attr.u32Height, (uint32_t)200);
  EXPECT_EQ(vpssconfig.chn_attr.u32Width, (uint32_t)200);
  EXPECT_EQ(vpssconfig.chn_attr.enPixelFormat, PIXEL_FORMAT_ARGB_1555);
  EXPECT_EQ(vpssconfig.chn_attr.stNormalize.bEnable, false);
}

TEST_F(FaceQualityTestSuite, skip_vpss_preprocess) {
  std::string image_path =
      (m_image_dir / std::string(m_json_object["reg_config"][0]["test_images"][0])).string();
  Image image(image_path, PIXEL_FORMAT_RGB_888);
  ASSERT_TRUE(image.open());

  {
    // test inference with skip vpss = false
    TDLModelHandler tdlmodel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_FACEQUALITY,
                             m_model_path.c_str(), false);
    ASSERT_NO_FATAL_FAILURE(tdlmodel.open());

    TDLObject<cvtdl_face_t> face_meta;
    init_face_meta(face_meta, 1);
    ASSERT_EQ(CVI_TDL_FaceQuality(m_tdl_handle, image.getFrame(), face_meta, NULL),
              CVI_TDL_SUCCESS);
    EXPECT_TRUE(face_meta->info[0].face_quality != -1.0);
  }

  {
    // test inference with skip vpss = true
    TDLModelHandler tdlmodel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_FACEQUALITY,
                             m_model_path.c_str(), true);
    ASSERT_NO_FATAL_FAILURE(tdlmodel.open());

    TDLObject<cvtdl_face_t> face_meta;
    init_face_meta(face_meta, 1);

    // This operatation shoule be failed, because face quality needs vpss preprocessing.
    ASSERT_EQ(CVI_TDL_FaceQuality(m_tdl_handle, image.getFrame(), face_meta, NULL),
              CVI_TDL_ERR_INVALID_ARGS);
  }
}

TEST_F(FaceQualityTestSuite, inference) {
  TDLModelHandler tdlmodel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_FACEQUALITY, m_model_path.c_str(),
                           false);
  ASSERT_NO_FATAL_FAILURE(tdlmodel.open());

  std::string image_path =
      (m_image_dir / std::string(m_json_object["reg_config"][0]["test_images"][0])).string();

  // test inference for PIXEL_FORMAT_RGB_888 format.
  {
    Image image_rgb(image_path, PIXEL_FORMAT_RGB_888);
    ASSERT_TRUE(image_rgb.open());

    // test 1 face
    {
      TDLObject<cvtdl_face_t> face_meta;
      init_face_meta(face_meta, 1);
      ASSERT_EQ(CVI_TDL_FaceQuality(m_tdl_handle, image_rgb.getFrame(), face_meta, NULL),
                CVI_TDL_SUCCESS);
      EXPECT_TRUE(face_meta->info[0].face_quality != -1.0);
    }

    // test 10 faces
    {
      TDLObject<cvtdl_face_t> face_meta;
      init_face_meta(face_meta, 10);
      ASSERT_EQ(CVI_TDL_FaceQuality(m_tdl_handle, image_rgb.getFrame(), face_meta, NULL),
                CVI_TDL_SUCCESS);
      EXPECT_TRUE(face_meta->info[0].face_quality != -1.0);
      float score = face_meta->info[0].face_quality;
      for (uint32_t fid = 1; fid < 10; fid++) {
        EXPECT_FLOAT_EQ(score, face_meta->info[fid].face_quality);
      }
    }

    // test inference with skipping faces
    {
      TDLObject<cvtdl_face_t> face_meta;
      init_face_meta(face_meta, 10);
      bool skipped_faces[10] = {true, false, false, true, false, false, true, false, false, true};

      ASSERT_EQ(CVI_TDL_FaceQuality(m_tdl_handle, image_rgb.getFrame(), face_meta, skipped_faces),
                CVI_TDL_SUCCESS);

      for (uint32_t fid = 0; fid < 10; fid++) {
        if (skipped_faces[fid]) {
          EXPECT_TRUE(face_meta->info[fid].face_quality == -1.0);
        } else {
          EXPECT_TRUE(face_meta->info[fid].face_quality != -1.0);
        }
      }
    }
  }

  // inference for PIXEL_FORMAT_RGB_888_PLANAR format.
  {
    Image image_rgb(image_path, PIXEL_FORMAT_RGB_888_PLANAR);
    ASSERT_TRUE(image_rgb.open());
    TDLObject<cvtdl_face_t> face_meta;
    init_face_meta(face_meta, 1);

    ASSERT_EQ(CVI_TDL_FaceQuality(m_tdl_handle, image_rgb.getFrame(), face_meta, NULL),
              CVI_TDL_SUCCESS);
  }
}

TEST_F(FaceQualityTestSuite, accruacy) {
  TDLModelHandler tdlmodel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_FACEQUALITY, m_model_path.c_str(),
                           false);
  ASSERT_NO_FATAL_FAILURE(tdlmodel.open());

  std::ofstream m_ofs_results;

  int img_num = int(m_json_object["reg_config"][0]["image_num"]);

  float pos_threshold = float(m_json_object["reg_config"][0]["pos_threshold"]);
  float neg_threshold = float(m_json_object["reg_config"][0]["neg_threshold"]);

  for (int img_idx = 0; img_idx < img_num; img_idx++) {
    std::string image_path =
        (m_image_dir / std::string(m_json_object["reg_config"][0]["test_images"][img_idx]))
            .string();
    Image image_rgb(image_path, PIXEL_FORMAT_RGB_888);
    ASSERT_TRUE(image_rgb.open());

    int expected_res = int(m_json_object["reg_config"][0]["expected_results"][img_idx]);

    TDLObject<cvtdl_face_t> face_meta;
    init_face_meta(face_meta, 1);

    ASSERT_EQ(CVI_TDL_FaceQuality(m_tdl_handle, image_rgb.getFrame(), face_meta, NULL),
              CVI_TDL_SUCCESS);

    // printf("face quality: %f (expected: %d)\n", face_meta.info[0].face_quality, expected_res);
    if (expected_res == 0) {
      EXPECT_LT(face_meta->info[0].face_quality, neg_threshold)
          << "image path: " << image_path << "\n"
          << "model path: " << m_model_path;
    } else {
      EXPECT_GT(face_meta->info[0].face_quality, pos_threshold)
          << "image path: " << image_path << "\n"
          << "model path: " << m_model_path;
    }
    CVI_TDL_FreeCpp(face_meta);
  }
}

}  // namespace unitest
}  // namespace cvitdl
