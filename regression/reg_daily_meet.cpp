#include <fstream>
#include <string>
#include <unordered_map>

#include <gtest.h>
#include "core/utils/vpss_helper.h"
#include "cvi_tdl.h"
#include "cvi_tdl_evaluation.h"
#include "cvi_tdl_media.h"
#include "cvi_tdl_test.hpp"
#include "json.hpp"
#include "raii.hpp"
#include "regression_utils.hpp"
namespace fs = std::experimental::filesystem;
namespace cvitdl {
namespace unitest {

class Meeting_DetectionTestSuite : public CVI_TDLModelTestSuite {
 public:
  Meeting_DetectionTestSuite()
      : CVI_TDLModelTestSuite("reg_daily_meetdet.json", "reg_daily_meetdet") {}

  virtual ~Meeting_DetectionTestSuite() = default;

  std::string m_model_path;

 protected:
  virtual void SetUp() {
    m_tdl_handle = NULL;
    ASSERT_EQ(CVI_TDL_CreateHandle2(&m_tdl_handle, 1, 0), CVI_TDL_SUCCESS);
    ASSERT_EQ(CVI_TDL_SetVpssTimeout(m_tdl_handle, 1000), CVI_TDL_SUCCESS);
  }

  virtual void TearDown() {
    CVI_TDL_DestroyHandle(m_tdl_handle);
    m_tdl_handle = NULL;
  }
};

TEST_F(Meeting_DetectionTestSuite, open_close_model) {
  std::string model_name = std::string(m_json_object["model_name"]);
  m_model_path = (m_model_dir / fs::path(model_name)).string();
  ASSERT_EQ(CVI_TDL_OpenModel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_HAND_FACE_PERSON_DETECTION,
                              m_model_path.c_str()),
            CVI_TDL_SUCCESS)
      << "failed to set model path: " << m_model_path.c_str();

  const char *model_path_get =
      CVI_TDL_GetModelPath(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_HAND_FACE_PERSON_DETECTION);

  EXPECT_PRED2([](auto s1, auto s2) { return s1 == s2; }, m_model_path,
               std::string(model_path_get));

  ASSERT_EQ(CVI_TDL_CloseModel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_HAND_FACE_PERSON_DETECTION),
            CVI_TDL_SUCCESS);
}

TEST_F(Meeting_DetectionTestSuite, inference) {
  std::string model_name = std::string(m_json_object["model_name"]);
  m_model_path = (m_model_dir / fs::path(model_name)).string();
  ASSERT_EQ(CVI_TDL_OpenModel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_HAND_FACE_PERSON_DETECTION,
                              m_model_path.c_str()),
            CVI_TDL_SUCCESS);

  auto results = m_json_object["results"];
  const float bbox_threshold = 0.90;
  const float score_threshold = 0.1;
  std::string image_path = (m_image_dir / results.begin().key()).string();

  Image image(image_path, PIXEL_FORMAT_RGB_888);
  ASSERT_TRUE(image.open());
  VIDEO_FRAME_INFO_S *vframe = image.getFrame();
  TDLObject<cvtdl_object_t> meet_meta;
  init_obj_meta(meet_meta, 1, vframe->stVFrame.u32Height, vframe->stVFrame.u32Width, 0);
  ASSERT_EQ(CVI_TDL_HandFacePerson_Detection(m_tdl_handle, vframe, meet_meta), CVI_TDL_SUCCESS);

  ASSERT_EQ(CVI_TDL_CloseModel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_HAND_FACE_PERSON_DETECTION),
            CVI_TDL_SUCCESS);
}

TEST_F(Meeting_DetectionTestSuite, accuracy) {
  std::string model_name = std::string(m_json_object["model_name"]);
  m_model_path = (m_model_dir / fs::path(model_name)).string();
  ASSERT_EQ(CVI_TDL_OpenModel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_HAND_FACE_PERSON_DETECTION,
                              m_model_path.c_str()),
            CVI_TDL_SUCCESS);

  auto results = m_json_object["results"];
  const float bbox_threshold = 0.90;
  const float score_threshold = 0.1;
  for (nlohmann::json::iterator iter = results.begin(); iter != results.end(); iter++) {
    std::string image_path = (m_image_dir / iter.key()).string();

    Image image(image_path, PIXEL_FORMAT_RGB_888);
    ASSERT_TRUE(image.open());
    VIDEO_FRAME_INFO_S *vframe = image.getFrame();
    TDLObject<cvtdl_object_t> meet_meta;
    init_obj_meta(meet_meta, 1, vframe->stVFrame.u32Height, vframe->stVFrame.u32Width, 0);
    ASSERT_EQ(CVI_TDL_HandFacePerson_Detection(m_tdl_handle, vframe, meet_meta), CVI_TDL_SUCCESS);

    auto expected_dets = iter.value();

    for (uint32_t det_index = 0; det_index < expected_dets.size(); det_index++) {
      auto bbox = expected_dets[det_index]["bbox"];
      int catId = int(expected_dets[det_index]["category_id"]);

      cvtdl_bbox_t expected_bbox = {
          .x1 = float(bbox[0]),
          .y1 = float(bbox[1]),
          .x2 = float(bbox[2]),
          .y2 = float(bbox[3]),
          .score = float(expected_dets[det_index]["score"]),
      };

      auto comp = [=](cvtdl_object_info_t &info, cvtdl_bbox_t &bbox) {
        if (info.classes == catId && iou(info.bbox, bbox) >= bbox_threshold &&
            abs(info.bbox.score - bbox.score) <= score_threshold) {
          return true;
        }
        return false;
      };
      EXPECT_TRUE(match_dets(*meet_meta, expected_bbox, comp))
          << "Error!"
          << "\n"
          << "expected bbox: (" << expected_bbox.x1 << ", " << expected_bbox.y1 << ", "
          << expected_bbox.x2 << ", " << expected_bbox.y2 << ")\n"
          << "score: " << expected_bbox.score << "\n"
          << "expectedID: " << catId << "\n"
          << "[" << meet_meta->info[det_index].bbox.x1 << "," << meet_meta->info[det_index].bbox.y1
          << "," << meet_meta->info[det_index].bbox.x2 << "," << meet_meta->info[det_index].bbox.y2
          << "," << meet_meta->info[det_index].classes << ","
          << meet_meta->info[det_index].bbox.score << "],\n";
    }
    CVI_TDL_FreeCpp(meet_meta);  // delete expected_res;
  }
  ASSERT_EQ(CVI_TDL_CloseModel(m_tdl_handle, CVI_TDL_SUPPORTED_MODEL_HAND_FACE_PERSON_DETECTION),
            CVI_TDL_SUCCESS);
}

}  // namespace unitest
}  // namespace cvitdl
