#include "version.hpp"

#include "alphapose/alphapose.hpp"
#include "core/core/cvtdl_errno.h"
#include "core/cvi_tdl_core.h"
#include "core/cvi_tdl_types_mem_internal.h"
#include "cvi_tdl_core_internal.hpp"
#include "cvi_tdl_experimental.h"
#include "cvi_tdl_log.hpp"
#include "deepsort/cvi_deepsort.hpp"
#include "eye_classification/eye_classification.hpp"
#include "face_attribute/face_attribute.hpp"
#include "face_landmarker/dms_landmark.hpp"
#include "face_landmarker/face_landmark_det3.hpp"
#include "face_landmarker/face_landmarker.hpp"
#include "face_landmarker/face_landmarker_det2.hpp"
#include "face_mask_detection/retinaface_yolox.hpp"
#include "face_quality/face_quality.hpp"
#include "fall_detection/fall_det_monitor.hpp"
#include "fall_detection/fall_detection.hpp"
#include "hand_classification/hand_classification.hpp"
#include "hand_keypoint/hand_keypoint.hpp"
#include "hand_keypoint_classification/hand_keypoint_classification.hpp"
#include "human_keypoints_detection/hrnet/hrnet.hpp"
#include "human_keypoints_detection/simcc/simcc.hpp"
#include "human_keypoints_detection/yolov8_pose/yolov8_pose.hpp"
#include "image_classification/image_classification.hpp"
#include "incar_object_detection/incar_object_detection.hpp"
#include "license_plate_detection/license_plate_detection.hpp"
#include "license_plate_recognition/license_plate_recognition.hpp"
#include "license_plate_recognitionv2/license_plate_recognitionv2.hpp"
#include "liveness/ir_liveness.hpp"
#include "liveness/liveness.hpp"
#include "mask_classification/mask_classification.hpp"
#include "mask_face_recognition/mask_face_recognition.hpp"
#include "motion_detection/md.hpp"
#include "object_detection/mobiledetv2/mobiledetv2.hpp"
#include "object_detection/ppyoloe/ppyoloe.hpp"
#include "object_detection/yolo/yolo.hpp"
#include "object_detection/yolov3/yolov3.hpp"
#include "object_detection/yolov5/yolov5.hpp"
#include "object_detection/yolov6/yolov6.hpp"
#include "object_detection/yolov8/yolov8.hpp"
#include "object_detection/yolox/yolox.hpp"
#include "osnet/osnet.hpp"
#include "retina_face/retina_face.hpp"
#include "retina_face/scrfd_face.hpp"
#include "segmentation/deeplabv3.hpp"
#include "smoke_classification/smoke_classification.hpp"
#include "sound_classification/sound_classification.hpp"
#include "sound_classification/sound_classification_v2.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "thermal_face_detection/thermal_face.hpp"
#include "thermal_person_detection/thermal_person.hpp"
#include "utils/core_utils.hpp"
#include "utils/image_utils.hpp"
#include "yawn_classification/yawn_classification.hpp"

using namespace std;
using namespace cvitdl;

struct ModelParams {
  VpssEngine *vpss_engine;
  uint32_t vpss_timeout_value;
};

using CreatorFunc = std::function<Core *(const ModelParams &)>;
using CreatorFuncAud = std::function<Core *()>;
using namespace std::placeholders;

template <typename C, typename... Args>
Core *create_model(const ModelParams &params, Args... arg) {
  C *instance = new C(arg...);

  instance->setVpssEngine(params.vpss_engine);
  instance->setVpssTimeout(params.vpss_timeout_value);
  return instance;
}

template <typename C>
Core *create_model_aud() {
  C *instance = new C();
  return instance;
}

static int createIVEHandleIfNeeded(cvitdl_context_t *ctx) {
  if (ctx->ive_handle == nullptr) {
    ctx->ive_handle = new ive::IVE;
    if (ctx->ive_handle->init() != CVI_SUCCESS) {
      LOGC("IVE handle init failed, please insmod cv18?x_ive.ko.\n");
      return CVI_FAILURE;
    }
  }
  return CVI_SUCCESS;
}

static CVI_S32 initVPSSIfNeeded(cvitdl_context_t *ctx, CVI_TDL_SUPPORTED_MODEL_E model_id) {
  bool skipped;
  CVI_S32 ret = CVI_TDL_GetSkipVpssPreprocess(ctx, model_id, &skipped);
  if (ret != CVI_TDL_SUCCESS) {
    return ret;
  }

  // Don't create vpss if preprocessing is skipped.
  if (skipped) {
    return CVI_TDL_SUCCESS;
  }

  if (ctx->vec_vpss_engine.size() == 0) {
    return CVI_TDL_SUCCESS;
  }

  uint32_t thread;
  ret = CVI_TDL_GetVpssThread(ctx, model_id, &thread);
  if (ret == CVI_TDL_SUCCESS) {
    if (!ctx->vec_vpss_engine[thread]->isInitialized()) {
      ret = ctx->vec_vpss_engine[thread]->init();
    }
  }
  return ret;
}

// Convenience macros for creator
#define CREATOR(type) CreatorFunc(create_model<type>)

// Convenience macros for creator
#define CREATOR_AUD(type) CreatorFuncAud(create_model_aud<type>)

// Convenience macros for creator, P{NUM} stands for how many parameters for creator
#define CREATOR_P1(type, arg_type, arg1) \
  CreatorFunc(std::bind(create_model<type, arg_type>, _1, arg1))

/**
 * IMPORTANT!!
 * Creators for all DNN model. Please remember to register model creator here, or
 * TDL SDK cannot instantiate model correctly.
 */
unordered_map<int, CreatorFuncAud> MODEL_CREATORS_AUD = {
    {CVI_TDL_SUPPORTED_MODEL_SOUNDCLASSIFICATION_V2, CREATOR_AUD(SoundClassificationV2)},
};

unordered_map<int, CreatorFunc> MODEL_CREATORS = {
    {CVI_TDL_SUPPORTED_MODEL_FACEQUALITY, CREATOR(FaceQuality)},
    {CVI_TDL_SUPPORTED_MODEL_THERMALFACE, CREATOR(ThermalFace)},
    {CVI_TDL_SUPPORTED_MODEL_THERMALPERSON, CREATOR(ThermalPerson)},
    {CVI_TDL_SUPPORTED_MODEL_LIVENESS, CREATOR(Liveness)},
    {CVI_TDL_SUPPORTED_MODEL_IRLIVENESS, CREATOR(IrLiveness)},
    {CVI_TDL_SUPPORTED_MODEL_MASKCLASSIFICATION, CREATOR(MaskClassification)},
    {CVI_TDL_SUPPORTED_MODEL_HANDCLASSIFICATION, CREATOR(HandClassification)},
    {CVI_TDL_SUPPORTED_MODEL_HAND_KEYPOINT, CREATOR(HandKeypoint)},
    {CVI_TDL_SUPPORTED_MODEL_HAND_KEYPOINT_CLASSIFICATION, CREATOR(HandKeypointClassification)},
    {CVI_TDL_SUPPORTED_MODEL_YOLOV3, CREATOR(Yolov3)},
    {CVI_TDL_SUPPORTED_MODEL_YOLOV5, CREATOR(Yolov5)},
    {CVI_TDL_SUPPORTED_MODEL_YOLOV6, CREATOR(Yolov6)},
    {CVI_TDL_SUPPORTED_MODEL_YOLOV7, CREATOR(Yolov5)},
    {CVI_TDL_SUPPORTED_MODEL_YOLO, CREATOR(Yolo)},
    {CVI_TDL_SUPPORTED_MODEL_YOLOX, CREATOR(YoloX)},
    {CVI_TDL_SUPPORTED_MODEL_PPYOLOE, CREATOR(PPYoloE)},
    {CVI_TDL_SUPPORTED_MODEL_FACEMASKDETECTION, CREATOR(RetinafaceYolox)},
    {CVI_TDL_SUPPORTED_MODEL_OSNET, CREATOR(OSNet)},
    {CVI_TDL_SUPPORTED_MODEL_SOUNDCLASSIFICATION, CREATOR(SoundClassification)},
    {CVI_TDL_SUPPORTED_MODEL_WPODNET, CREATOR(LicensePlateDetection)},
    {CVI_TDL_SUPPORTED_MODEL_LP_RECONGNITION, CREATOR(LicensePlateRecognitionV2)},
    {CVI_TDL_SUPPORTED_MODEL_DEEPLABV3, CREATOR(Deeplabv3)},
    {CVI_TDL_SUPPORTED_MODEL_ALPHAPOSE, CREATOR(AlphaPose)},
    {CVI_TDL_SUPPORTED_MODEL_EYECLASSIFICATION, CREATOR(EyeClassification)},
    {CVI_TDL_SUPPORTED_MODEL_YAWNCLASSIFICATION, CREATOR(YawnClassification)},
    {CVI_TDL_SUPPORTED_MODEL_SMOKECLASSIFICATION, CREATOR(SmokeClassification)},
    {CVI_TDL_SUPPORTED_MODEL_FACELANDMARKER, CREATOR(FaceLandmarker)},
    {CVI_TDL_SUPPORTED_MODEL_FACELANDMARKERDET2, CREATOR(FaceLandmarkerDet2)},
    {CVI_TDL_SUPPORTED_MODEL_INCAROBJECTDETECTION, CREATOR(IncarObjectDetection)},
    {CVI_TDL_SUPPORTED_MODEL_MASKFACERECOGNITION, CREATOR(MaskFaceRecognition)},
    {CVI_TDL_SUPPORTED_MODEL_SCRFDFACE, CREATOR(ScrFDFace)},
    {CVI_TDL_SUPPORTED_MODEL_RETINAFACE, CREATOR_P1(RetinaFace, PROCESS, CAFFE)},
    {CVI_TDL_SUPPORTED_MODEL_RETINAFACE_IR, CREATOR_P1(RetinaFace, PROCESS, PYTORCH)},
    {CVI_TDL_SUPPORTED_MODEL_RETINAFACE_HARDHAT, CREATOR_P1(RetinaFace, PROCESS, PYTORCH)},
    {CVI_TDL_SUPPORTED_MODEL_FACEATTRIBUTE, CREATOR_P1(FaceAttribute, bool, true)},
    {CVI_TDL_SUPPORTED_MODEL_FACERECOGNITION, CREATOR_P1(FaceAttribute, bool, false)},
    {CVI_TDL_SUPPORTED_MODEL_LPRNET_TW, CREATOR_P1(LicensePlateRecognition, LP_FORMAT, TAIWAN)},
    {CVI_TDL_SUPPORTED_MODEL_LPRNET_CN, CREATOR_P1(LicensePlateRecognition, LP_FORMAT, CHINA)},
    {CVI_TDL_SUPPORTED_MODEL_HAND_DETECTION,
     CREATOR_P1(YoloV8Detection, PAIR_INT, std::make_pair(64, 1))},
    {CVI_TDL_SUPPORTED_MODEL_PERSON_PETS_DETECTION,
     CREATOR_P1(YoloV8Detection, PAIR_INT, std::make_pair(64, 3))},
    {CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION,
     CREATOR_P1(YoloV8Detection, PAIR_INT, std::make_pair(64, 80))},
    {CVI_TDL_SUPPORTED_MODEL_PERSON_VEHICLE_DETECTION,
     CREATOR_P1(YoloV8Detection, PAIR_INT, std::make_pair(64, 7))},
    {CVI_TDL_SUPPORTED_MODEL_HAND_FACE_PERSON_DETECTION,
     CREATOR_P1(YoloV8Detection, PAIR_INT, std::make_pair(64, 3))},
    {CVI_TDL_SUPPORTED_MODEL_HEAD_PERSON_DETECTION,
     CREATOR_P1(YoloV8Detection, PAIR_INT, std::make_pair(64, 2))},
    {CVI_TDL_SUPPORTED_MODEL_YOLOV8POSE,
     CREATOR_P1(YoloV8Pose, TUPLE_INT, std::make_tuple(64, 17, 1))},
    {CVI_TDL_SUPPORTED_MODEL_LP_DETECTION,
     CREATOR_P1(YoloV8Pose, TUPLE_INT, std::make_tuple(64, 4, 2))},
    {CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_COCO80,
     CREATOR_P1(MobileDetV2, MobileDetV2::Category, MobileDetV2::Category::coco80)},
    {CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PERSON_VEHICLE,
     CREATOR_P1(MobileDetV2, MobileDetV2::Category, MobileDetV2::Category::person_vehicle)},
    {CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_VEHICLE,
     CREATOR_P1(MobileDetV2, MobileDetV2::Category, MobileDetV2::Category::vehicle)},
    {CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN,
     CREATOR_P1(MobileDetV2, MobileDetV2::Category, MobileDetV2::Category::pedestrian)},
    {CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PERSON_PETS,
     CREATOR_P1(MobileDetV2, MobileDetV2::Category, MobileDetV2::Category::person_pets)},
    {CVI_TDL_SUPPORTED_MODEL_SIMCC_POSE, CREATOR(Simcc)},
    {CVI_TDL_SUPPORTED_MODEL_HRNET_POSE, CREATOR(Hrnet)},
    {CVI_TDL_SUPPORTED_MODEL_LANDMARK_DET3, CREATOR(FaceLandmarkDet3)},
    {CVI_TDL_SUPPORTED_MODEL_DMSLANDMARKERDET, CREATOR(DMSLandmarkerDet)},
    {CVI_TDL_SUPPORTED_MODEL_IMAGE_CLASSIFICATION, CREATOR(ImageClassification)},
};

//*************************************************
// Experimental features
void CVI_TDL_EnableGDC(cvitdl_handle_t handle, bool use_gdc) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  ctx->use_gdc_wrap = use_gdc;
  LOGI("Experimental feature GDC hardware %s.\n", use_gdc ? "enabled" : "disabled");
}
//*************************************************

inline void __attribute__((always_inline)) removeCtx(cvitdl_context_t *ctx) {
  delete ctx->ds_tracker;
  delete ctx->td_model;
  delete ctx->md_model;

  if (ctx->ive_handle) {
    ctx->ive_handle->destroy();
  }

  for (auto it : ctx->vec_vpss_engine) {
    delete it;
  }
  delete ctx;
}

inline Core *__attribute__((always_inline))
getInferenceInstance(const CVI_TDL_SUPPORTED_MODEL_E index, cvitdl_context_t *ctx) {
  cvitdl_model_t &m_t = ctx->model_cont[index];
  if (m_t.instance == nullptr) {
    // create custom instance here
    if (index == CVI_TDL_SUPPORTED_MODEL_SOUNDCLASSIFICATION_V2) {
      if (MODEL_CREATORS_AUD.find(index) == MODEL_CREATORS_AUD.end()) {
        LOGE("Cannot find creator for %s, Please register a creator for this model!\n",
             CVI_TDL_GetModelName(index));
        return nullptr;
      }
      auto creator = MODEL_CREATORS_AUD[index];
      m_t.instance = creator();
    } else {
      if (MODEL_CREATORS.find(index) == MODEL_CREATORS.end()) {
        LOGE("Cannot find creator for %s, Please register a creator for this model!\n",
             CVI_TDL_GetModelName(index));
        return nullptr;
      }

      auto creator = MODEL_CREATORS[index];
      ModelParams params = {.vpss_engine = ctx->vec_vpss_engine[m_t.vpss_thread],
                            .vpss_timeout_value = ctx->vpss_timeout_value};

      m_t.instance = creator(params);
      m_t.instance->setVpssEngine(ctx->vec_vpss_engine[m_t.vpss_thread]);
      m_t.instance->setVpssTimeout(ctx->vpss_timeout_value);
    }
  }
  return m_t.instance;
}

CVI_S32 CVI_TDL_CreateHandle(cvitdl_handle_t *handle) {
  return CVI_TDL_CreateHandle2(handle, -1, 0);
}

CVI_S32 CVI_TDL_CreateHandle2(cvitdl_handle_t *handle, const VPSS_GRP vpssGroupId,
                              const CVI_U8 vpssDev) {
  if (vpssGroupId < -1 || vpssGroupId >= VPSS_MAX_GRP_NUM) {
    LOGE("Invalid Vpss Grp: %d.\n", vpssGroupId);
    return CVI_TDL_ERR_INIT_VPSS;
  }

  cvitdl_context_t *ctx = new cvitdl_context_t;
  ctx->ive_handle = NULL;
  ctx->vec_vpss_engine.push_back(new VpssEngine(vpssGroupId, vpssDev));
  const char timestamp[] = __DATE__ " " __TIME__;
  LOGI("cvitdl_handle_t is created, version %s-%s", CVI_TDL_TAG, timestamp);
  *handle = ctx;
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_CreateHandle3(cvitdl_handle_t *handle) {
  cvitdl_context_t *ctx = new cvitdl_context_t;
  ctx->ive_handle = NULL;
  const char timestamp[] = __DATE__ " " __TIME__;
  LOGI("cvitdl_handle_t is created, version %s-%s", CVI_TDL_TAG, timestamp);
  *handle = ctx;
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_DestroyHandle(cvitdl_handle_t handle) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  CVI_TDL_CloseAllModel(handle);
  removeCtx(ctx);
  LOGI("cvitdl_handle_t is destroyed.");
  return CVI_TDL_SUCCESS;
}

static bool checkModelFile(const char *filepath) {
  struct stat buffer;
  bool ret = false;
  if (stat(filepath, &buffer) == 0) {
    if (S_ISREG(buffer.st_mode)) {
      ret = true;
    } else {
      LOGE("Path of model file isn't a regular file: %s\n", filepath);
    }
  } else {
    LOGE("Model file doesn't exists: %s\n", filepath);
  }
  return ret;
}

CVI_S32 CVI_TDL_OpenModel(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                          const char *filepath) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  cvitdl_model_t &m_t = ctx->model_cont[config];
  Core *instance = getInferenceInstance(config, ctx);

  if (instance != nullptr) {
    if (instance->isInitialized()) {
      LOGW("%s: Inference has already initialized. Please call CVI_TDL_CloseModel to reset.\n",
           CVI_TDL_GetModelName(config));
      return CVI_TDL_ERR_MODEL_INITIALIZED;
    }
  } else {
    LOGE("Cannot create model: %s\n", CVI_TDL_GetModelName(config));
    return CVI_TDL_ERR_OPEN_MODEL;
  }

  if (!checkModelFile(filepath)) {
    return CVI_TDL_ERR_INVALID_MODEL_PATH;
  }

  m_t.model_path = filepath;
  CVI_S32 ret = m_t.instance->modelOpen(m_t.model_path.c_str());
  if (ret != CVI_TDL_SUCCESS) {
    LOGE("Failed to open model: %s (%s)", CVI_TDL_GetModelName(config), m_t.model_path.c_str());
    return ret;
  }
  LOGI("Model is opened successfully: %s \n", CVI_TDL_GetModelName(config));
  return CVI_TDL_SUCCESS;
}

const char *CVI_TDL_GetModelPath(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  return GetModelName(ctx->model_cont[config]);
}

CVI_S32 CVI_TDL_SetSkipVpssPreprocess(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                                      bool skip) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Core *instance = getInferenceInstance(config, ctx);
  if (instance != nullptr) {
    instance->skipVpssPreprocess(skip);
  } else {
    LOGE("Cannot create model: %s\n", CVI_TDL_GetModelName(config));
    return CVI_TDL_ERR_OPEN_MODEL;
  }
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_SetPerfEvalInterval(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                                    int interval) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Core *instance = getInferenceInstance(config, ctx);
  if (instance != nullptr) {
    instance->set_perf_eval_interval(interval);
  } else {
    LOGE("Cannot create model: %s\n", CVI_TDL_GetModelName(config));
    return CVI_TDL_ERR_OPEN_MODEL;
  }
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_GetSkipVpssPreprocess(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                                      bool *skip) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Core *instance = getInferenceInstance(config, ctx);
  if (instance != nullptr) {
    *skip = instance->hasSkippedVpssPreprocess();
  } else {
    LOGE("Cannot create model: %s\n", CVI_TDL_GetModelName(config));
    return CVI_TDL_ERR_OPEN_MODEL;
  }
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_SetModelThreshold(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                                  float threshold) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Core *instance = getInferenceInstance(config, ctx);
  if (instance != nullptr) {
    instance->setModelThreshold(threshold);
  } else {
    LOGE("Cannot create model: %s\n", CVI_TDL_GetModelName(config));
    return CVI_TDL_ERR_OPEN_MODEL;
  }
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_GetModelThreshold(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                                  float *threshold) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Core *instance = getInferenceInstance(config, ctx);
  if (instance != nullptr) {
    *threshold = instance->getModelThreshold();
  } else {
    LOGE("Cannot create model: %s\n", CVI_TDL_GetModelName(config));
    return CVI_TDL_ERR_OPEN_MODEL;
  }
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_SetModelNmsThreshold(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                                     float threshold) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Core *instance = getInferenceInstance(config, ctx);
  if (instance != nullptr) {
    instance->setModelNmsThreshold(threshold);
  } else {
    LOGE("Cannot create model: %s\n", CVI_TDL_GetModelName(config));
    return CVI_TDL_ERR_OPEN_MODEL;
  }
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_GetModelNMmsThreshold(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                                      float *threshold) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Core *instance = getInferenceInstance(config, ctx);
  if (instance != nullptr) {
    *threshold = instance->getModelNmsThreshold();
  } else {
    LOGE("Cannot create model: %s\n", CVI_TDL_GetModelName(config));
    return CVI_TDL_ERR_OPEN_MODEL;
  }
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_UseMmap(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E model, bool mmap) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Core *instance = getInferenceInstance(model, ctx);
  if (instance == nullptr) {
    return CVI_TDL_FAILURE;
  }
  instance->setUseMmap(mmap);
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_SetVpssThread(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                              const uint32_t thread) {
  return CVI_TDL_SetVpssThread2(handle, config, thread, -1, 0);
}

CVI_S32 CVI_TDL_SetVpssThread2(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                               const uint32_t thread, const VPSS_GRP vpssGroupId,
                               const CVI_U8 dev) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Core *instance = getInferenceInstance(config, ctx);
  if (instance != nullptr) {
    return setVPSSThread(ctx->model_cont[config], ctx->vec_vpss_engine, thread, vpssGroupId, dev);
  } else {
    LOGE("Cannot create model: %s\n", CVI_TDL_GetModelName(config));
    return CVI_TDL_ERR_OPEN_MODEL;
  }
}

CVI_S32 CVI_TDL_SetVBPool(cvitdl_handle_t handle, uint32_t thread, VB_POOL pool_id) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  if (thread >= ctx->vec_vpss_engine.size()) {
    LOGE("Invalid vpss thread: %d, should be 0 to %d\n", thread,
         static_cast<uint32_t>(ctx->vec_vpss_engine.size() - 1));
    return CVI_TDL_FAILURE;
  }
  ctx->vec_vpss_engine[thread]->attachVBPool(pool_id);
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_GetVBPool(cvitdl_handle_t handle, uint32_t thread, VB_POOL *pool_id) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  if (thread >= ctx->vec_vpss_engine.size()) {
    LOGE("Invalid vpss thread: %d, should be 0 to %d\n", thread,
         static_cast<uint32_t>(ctx->vec_vpss_engine.size() - 1));
    return CVI_TDL_FAILURE;
  }
  *pool_id = ctx->vec_vpss_engine[thread]->getVBPool();
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_GetVpssThread(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                              uint32_t *thread) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  *thread = ctx->model_cont[config].vpss_thread;
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_SetVpssDepth(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E model,
                             uint32_t input_id, uint32_t depth) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Core *instance = getInferenceInstance(model, ctx);
  if (instance != nullptr) {
    return instance->setVpssDepth(input_id, depth);
  } else {
    LOGE("Cannot create model: %s\n", CVI_TDL_GetModelName(model));
    return CVI_TDL_ERR_OPEN_MODEL;
  }
}

CVI_S32 CVI_TDL_GetVpssDepth(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E model,
                             uint32_t input_id, uint32_t *depth) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Core *instance = getInferenceInstance(model, ctx);
  if (instance != nullptr) {
    return instance->getVpssDepth(input_id, depth);
  } else {
    LOGE("Cannot create model: %s\n", CVI_TDL_GetModelName(model));
    return CVI_TDL_ERR_OPEN_MODEL;
  }
}

CVI_S32 CVI_TDL_GetVpssGrpIds(cvitdl_handle_t handle, VPSS_GRP **groups, uint32_t *num) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  VPSS_GRP *ids = (VPSS_GRP *)malloc(ctx->vec_vpss_engine.size() * sizeof(VPSS_GRP));
  for (size_t i = 0; i < ctx->vec_vpss_engine.size(); i++) {
    ids[i] = ctx->vec_vpss_engine[i]->getGrpId();
  }
  *groups = ids;
  *num = ctx->vec_vpss_engine.size();
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_SetVpssTimeout(cvitdl_handle_t handle, uint32_t timeout) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  ctx->vpss_timeout_value = timeout;

  for (auto &m_inst : ctx->model_cont) {
    if (m_inst.second.instance != nullptr) {
      m_inst.second.instance->setVpssTimeout(timeout);
    }
  }
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_CloseAllModel(cvitdl_handle_t handle) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  for (auto &m_inst : ctx->model_cont) {
    if (m_inst.second.instance != nullptr) {
      m_inst.second.instance->modelClose();
      LOGI("Model is closed: %s\n", CVI_TDL_GetModelName(m_inst.first));
      delete m_inst.second.instance;
      m_inst.second.instance = nullptr;
    }
  }
  for (auto &m_inst : ctx->custom_cont) {
    if (m_inst.instance != nullptr) {
      m_inst.instance->modelClose();
      delete m_inst.instance;
      m_inst.instance = nullptr;
    }
  }
  ctx->model_cont.clear();
  ctx->custom_cont.clear();
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_CloseModel(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  cvitdl_model_t &m_t = ctx->model_cont[config];
  if (m_t.instance == nullptr) {
    return CVI_TDL_ERR_CLOSE_MODEL;
  }

  m_t.instance->modelClose();
  LOGI("Model is closed: %s\n", CVI_TDL_GetModelName(config));
  delete m_t.instance;
  m_t.instance = nullptr;
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_SelectDetectClass(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                                  uint32_t num_selection, ...) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  va_list args;
  va_start(args, num_selection);

  std::vector<uint32_t> selected_classes;
  for (uint32_t i = 0; i < num_selection; i++) {
    uint32_t selected_class = va_arg(args, uint32_t);

    if (selected_class & CVI_TDL_DET_GROUP_MASK_HEAD) {
      uint32_t group_start = (selected_class & CVI_TDL_DET_GROUP_MASK_START) >> 16;
      uint32_t group_end = (selected_class & CVI_TDL_DET_GROUP_MASK_END);
      for (uint32_t i = group_start; i <= group_end; i++) {
        selected_classes.push_back(i);
      }
    } else {
      if (selected_class >= CVI_TDL_DET_TYPE_END) {
        LOGE("Invalid class id: %d\n", selected_class);
        return CVI_TDL_ERR_INVALID_ARGS;
      }
      selected_classes.push_back(selected_class);
    }
  }

  Core *instance = getInferenceInstance(config, ctx);
  if (instance != nullptr) {
    // TODO: only supports MobileDetV2 and YOLOX for now
    if (MobileDetV2 *mdetv2 = dynamic_cast<MobileDetV2 *>(instance)) {
      mdetv2->select_classes(selected_classes);
    } else {
      LOGW("CVI_TDL_SelectDetectClass only supports MobileDetV2for now.\n");
    }
  } else {
    LOGE("Failed to create model: %s\n", CVI_TDL_GetModelName(config));
    return CVI_TDL_ERR_OPEN_MODEL;
  }
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_GetVpssChnConfig(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                                 const CVI_U32 frameWidth, const CVI_U32 frameHeight,
                                 const CVI_U32 idx, cvtdl_vpssconfig_t *chnConfig) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  cvitdl::Core *instance = getInferenceInstance(config, ctx);
  if (instance == nullptr) {
    LOGE("Instance is null.\n");
    return CVI_TDL_ERR_OPEN_MODEL;
  }

  return instance->getChnConfig(frameWidth, frameHeight, idx, chnConfig);
}

CVI_S32 CVI_TDL_EnalbeDumpInput(cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E config,
                                const char *dump_path, bool enable) {
  CVI_S32 ret = CVI_TDL_SUCCESS;
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  cvitdl::Core *instance = getInferenceInstance(config, ctx);
  if (instance == nullptr) {
    LOGE("Instance is null.\n");
    return CVI_TDL_ERR_OPEN_MODEL;
  }

  instance->enableDebugger(enable);
  instance->setDebuggerOutputPath(dump_path);
  return ret;
}

/**
 *  Convenience macros for defining inference functions. F{NUM} stands for how many input frame
 *  variables, P{NUM} stands for how many input parameters in inference function. All inference
 *  function should follow same function signature, that is,
 *  CVI_S32 inference(Frame1, Frame2, ... FrameN, Param1, Param2, ..., ParamN)
 */
#define DEFINE_INF_FUNC_F1_P1(func_name, class_name, model_index, arg_type)                    \
  CVI_S32 func_name(const cvitdl_handle_t handle, VIDEO_FRAME_INFO_S *frame, arg_type arg1) {  \
    cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);                           \
    class_name *obj = dynamic_cast<class_name *>(getInferenceInstance(model_index, ctx));      \
    if (obj == nullptr) {                                                                      \
      LOGE("No instance found for %s.\n", #class_name);                                        \
      return CVI_TDL_ERR_OPEN_MODEL;                                                           \
    }                                                                                          \
    if (obj->isInitialized()) {                                                                \
      if (initVPSSIfNeeded(ctx, model_index) != CVI_SUCCESS) {                                 \
        return CVI_TDL_ERR_INIT_VPSS;                                                          \
      } else {                                                                                 \
        CVI_S32 ret = obj->inference(frame, arg1);                                             \
        if (ret != CVI_TDL_SUCCESS)                                                            \
          return ret;                                                                          \
        else                                                                                   \
          return obj->after_inference();                                                       \
      }                                                                                        \
    } else {                                                                                   \
      LOGE("Model (%s)is not yet opened! Please call CVI_TDL_OpenModel to initialize model\n", \
           CVI_TDL_GetModelName(model_index));                                                 \
      return CVI_TDL_ERR_NOT_YET_INITIALIZED;                                                  \
    }                                                                                          \
  }

#define DEFINE_INF_FUNC_F1_P2(func_name, class_name, model_index, arg1_type, arg2_type)        \
  CVI_S32 func_name(const cvitdl_handle_t handle, VIDEO_FRAME_INFO_S *frame, arg1_type arg1,   \
                    arg2_type arg2) {                                                          \
    cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);                           \
    class_name *obj = dynamic_cast<class_name *>(getInferenceInstance(model_index, ctx));      \
    if (obj == nullptr) {                                                                      \
      LOGE("No instance found for %s.\n", #class_name);                                        \
      return CVI_TDL_ERR_OPEN_MODEL;                                                           \
    }                                                                                          \
    if (obj->isInitialized()) {                                                                \
      if (initVPSSIfNeeded(ctx, model_index) != CVI_SUCCESS) {                                 \
        return CVI_TDL_ERR_INIT_VPSS;                                                          \
      } else {                                                                                 \
        CVI_S32 ret = obj->inference(frame, arg1, arg2);                                       \
        if (ret != CVI_TDL_SUCCESS)                                                            \
          return ret;                                                                          \
        else                                                                                   \
          return obj->after_inference();                                                       \
      }                                                                                        \
    } else {                                                                                   \
      LOGE("Model (%s)is not yet opened! Please call CVI_TDL_OpenModel to initialize model\n", \
           CVI_TDL_GetModelName(model_index));                                                 \
      return CVI_TDL_ERR_NOT_YET_INITIALIZED;                                                  \
    }                                                                                          \
  }

#define DEFINE_INF_FUNC_F2_P1(func_name, class_name, model_index, arg_type)                    \
  CVI_S32 func_name(const cvitdl_handle_t handle, VIDEO_FRAME_INFO_S *frame1,                  \
                    VIDEO_FRAME_INFO_S *frame2, arg_type arg1) {                               \
    cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);                           \
    class_name *obj = dynamic_cast<class_name *>(getInferenceInstance(model_index, ctx));      \
    if (obj == nullptr) {                                                                      \
      LOGE("No instance found for %s.\n", #class_name);                                        \
      return CVI_TDL_ERR_OPEN_MODEL;                                                           \
    }                                                                                          \
    if (obj->isInitialized()) {                                                                \
      if (initVPSSIfNeeded(ctx, model_index) != CVI_SUCCESS) {                                 \
        return CVI_TDL_ERR_INIT_VPSS;                                                          \
      } else {                                                                                 \
        CVI_S32 ret = obj->inference(frame1, frame2, arg1);                                    \
        if (ret != CVI_TDL_SUCCESS)                                                            \
          return ret;                                                                          \
        else                                                                                   \
          return obj->after_inference();                                                       \
      }                                                                                        \
    } else {                                                                                   \
      LOGE("Model (%s)is not yet opened! Please call CVI_TDL_OpenModel to initialize model\n", \
           CVI_TDL_GetModelName(model_index));                                                 \
      return CVI_TDL_ERR_NOT_YET_INITIALIZED;                                                  \
    }                                                                                          \
  }

#define DEFINE_INF_FUNC_F2_P2(func_name, class_name, model_index, arg1_type, arg2_type)        \
  CVI_S32 func_name(const cvitdl_handle_t handle, VIDEO_FRAME_INFO_S *frame1,                  \
                    VIDEO_FRAME_INFO_S *frame2, arg1_type arg1, arg2_type arg2) {              \
    cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);                           \
    class_name *obj = dynamic_cast<class_name *>(getInferenceInstance(model_index, ctx));      \
    if (obj == nullptr) {                                                                      \
      LOGE("No instance found for %s.\n", #class_name);                                        \
      return CVI_TDL_ERR_OPEN_MODEL;                                                           \
    }                                                                                          \
    if (obj->isInitialized()) {                                                                \
      if (initVPSSIfNeeded(ctx, model_index) != CVI_SUCCESS) {                                 \
        return CVI_TDL_ERR_INIT_VPSS;                                                          \
      } else {                                                                                 \
        CVI_S32 ret = obj->inference(frame1, frame2, arg1, arg2);                              \
        if (ret != CVI_TDL_SUCCESS)                                                            \
          return ret;                                                                          \
        else                                                                                   \
          return obj->after_inference();                                                       \
      }                                                                                        \
    } else {                                                                                   \
      LOGE("Model (%s)is not yet opened! Please call CVI_TDL_OpenModel to initialize model\n", \
           CVI_TDL_GetModelName(model_index));                                                 \
      return CVI_TDL_ERR_NOT_YET_INITIALIZED;                                                  \
    }                                                                                          \
  }

/**
 *  Define model inference function here.
 *
 *  IMPORTANT!!
 *  Please remember to register creator function in MODEL_CREATORS first, or TDL SDK cannot
 *  find a correct way to create model object.
 *
 */
DEFINE_INF_FUNC_F1_P1(CVI_TDL_DMSLDet, DMSLandmarkerDet, CVI_TDL_SUPPORTED_MODEL_DMSLANDMARKERDET,
                      cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_RetinaFace, RetinaFace, CVI_TDL_SUPPORTED_MODEL_RETINAFACE,
                      cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_ScrFDFace, ScrFDFace, CVI_TDL_SUPPORTED_MODEL_SCRFDFACE,
                      cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_FLDet3, FaceLandmarkDet3, CVI_TDL_SUPPORTED_MODEL_LANDMARK_DET3,
                      cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_RetinaFace_IR, RetinaFace, CVI_TDL_SUPPORTED_MODEL_RETINAFACE_IR,
                      cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_RetinaFace_Hardhat, RetinaFace,
                      CVI_TDL_SUPPORTED_MODEL_RETINAFACE_HARDHAT, cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_ThermalFace, ThermalFace, CVI_TDL_SUPPORTED_MODEL_THERMALFACE,
                      cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_ThermalPerson, ThermalPerson, CVI_TDL_SUPPORTED_MODEL_THERMALPERSON,
                      cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_FaceAttribute, FaceAttribute, CVI_TDL_SUPPORTED_MODEL_FACEATTRIBUTE,
                      cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P2(CVI_TDL_FaceAttributeOne, FaceAttribute,
                      CVI_TDL_SUPPORTED_MODEL_FACEATTRIBUTE, cvtdl_face_t *, int)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_FaceRecognition, FaceAttribute,
                      CVI_TDL_SUPPORTED_MODEL_FACERECOGNITION, cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P2(CVI_TDL_FaceRecognitionOne, FaceAttribute,
                      CVI_TDL_SUPPORTED_MODEL_FACERECOGNITION, cvtdl_face_t *, int)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_MaskFaceRecognition, MaskFaceRecognition,
                      CVI_TDL_SUPPORTED_MODEL_MASKFACERECOGNITION, cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P2(CVI_TDL_FaceQuality, FaceQuality, CVI_TDL_SUPPORTED_MODEL_FACEQUALITY,
                      cvtdl_face_t *, bool *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_MaskClassification, MaskClassification,
                      CVI_TDL_SUPPORTED_MODEL_MASKCLASSIFICATION, cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_HandClassification, HandClassification,
                      CVI_TDL_SUPPORTED_MODEL_HANDCLASSIFICATION, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_HandKeypoint, HandKeypoint, CVI_TDL_SUPPORTED_MODEL_HAND_KEYPOINT,
                      cvtdl_handpose21_meta_ts *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_HandKeypointClassification, HandKeypointClassification,
                      CVI_TDL_SUPPORTED_MODEL_HAND_KEYPOINT_CLASSIFICATION,
                      cvtdl_handpose21_meta_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_FaceMaskDetection, RetinafaceYolox,
                      CVI_TDL_SUPPORTED_MODEL_FACEMASKDETECTION, cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_MobileDetV2_Vehicle, MobileDetV2,
                      CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_VEHICLE, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_MobileDetV2_Pedestrian, MobileDetV2,
                      CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_MobileDetV2_Person_Vehicle, MobileDetV2,
                      CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PERSON_VEHICLE, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_MobileDetV2_Person_Pets, MobileDetV2,
                      CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_PERSON_PETS, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_MobileDetV2_COCO80, MobileDetV2,
                      CVI_TDL_SUPPORTED_MODEL_MOBILEDETV2_COCO80, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_Yolov3, Yolov3, CVI_TDL_SUPPORTED_MODEL_YOLOV3, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_Yolov5, Yolov5, CVI_TDL_SUPPORTED_MODEL_YOLOV5, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_Yolov6, Yolov6, CVI_TDL_SUPPORTED_MODEL_YOLOV6, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_Yolov7, Yolov5, CVI_TDL_SUPPORTED_MODEL_YOLOV7, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_Yolo, Yolo, CVI_TDL_SUPPORTED_MODEL_YOLO, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_YoloX, YoloX, CVI_TDL_SUPPORTED_MODEL_YOLOX, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_PPYoloE, PPYoloE, CVI_TDL_SUPPORTED_MODEL_PPYOLOE, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_OSNet, OSNet, CVI_TDL_SUPPORTED_MODEL_OSNET, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P2(CVI_TDL_OSNetOne, OSNet, CVI_TDL_SUPPORTED_MODEL_OSNET, cvtdl_object_t *, int)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_SoundClassification, SoundClassification,
                      CVI_TDL_SUPPORTED_MODEL_SOUNDCLASSIFICATION, int *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_SoundClassification_V2, SoundClassificationV2,
                      CVI_TDL_SUPPORTED_MODEL_SOUNDCLASSIFICATION_V2, int *)
DEFINE_INF_FUNC_F2_P1(CVI_TDL_DeeplabV3, Deeplabv3, CVI_TDL_SUPPORTED_MODEL_DEEPLABV3,
                      cvtdl_class_filter_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_LicensePlateRecognition_TW, LicensePlateRecognition,
                      CVI_TDL_SUPPORTED_MODEL_LPRNET_TW, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_LicensePlateRecognition_CN, LicensePlateRecognition,
                      CVI_TDL_SUPPORTED_MODEL_LPRNET_CN, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_LicensePlateDetection, LicensePlateDetection,
                      CVI_TDL_SUPPORTED_MODEL_WPODNET, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_AlphaPose, AlphaPose, CVI_TDL_SUPPORTED_MODEL_ALPHAPOSE,
                      cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_EyeClassification, EyeClassification,
                      CVI_TDL_SUPPORTED_MODEL_EYECLASSIFICATION, cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_YawnClassification, YawnClassification,
                      CVI_TDL_SUPPORTED_MODEL_YAWNCLASSIFICATION, cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_SmokeClassification, SmokeClassification,
                      CVI_TDL_SUPPORTED_MODEL_SMOKECLASSIFICATION, cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_FaceLandmarker, FaceLandmarker,
                      CVI_TDL_SUPPORTED_MODEL_FACELANDMARKER, cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_FaceLandmarkerDet2, FaceLandmarkerDet2,
                      CVI_TDL_SUPPORTED_MODEL_FACELANDMARKERDET2, cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_IncarObjectDetection, IncarObjectDetection,
                      CVI_TDL_SUPPORTED_MODEL_INCAROBJECTDETECTION, cvtdl_face_t *)
DEFINE_INF_FUNC_F2_P2(CVI_TDL_Liveness, Liveness, CVI_TDL_SUPPORTED_MODEL_LIVENESS, cvtdl_face_t *,
                      cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_IrLiveness, IrLiveness, CVI_TDL_SUPPORTED_MODEL_IRLIVENESS,
                      cvtdl_face_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_Yolov8_Pose, YoloV8Pose, CVI_TDL_SUPPORTED_MODEL_YOLOV8POSE,
                      cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_License_Plate_Detectionv2, YoloV8Pose,
                      CVI_TDL_SUPPORTED_MODEL_LP_DETECTION, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_License_Plate_Recognitionv2, LicensePlateRecognitionV2,
                      CVI_TDL_SUPPORTED_MODEL_LP_RECONGNITION, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_Simcc_Pose, Simcc, CVI_TDL_SUPPORTED_MODEL_SIMCC_POSE,
                      cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_Hrnet_Pose, Hrnet, CVI_TDL_SUPPORTED_MODEL_HRNET_POSE,
                      cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_Image_Classification, ImageClassification,
                      CVI_TDL_SUPPORTED_MODEL_IMAGE_CLASSIFICATION, cvtdl_class_meta_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_Hand_Detection, YoloV8Detection,
                      CVI_TDL_SUPPORTED_MODEL_HAND_DETECTION, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_PersonPet_Detection, YoloV8Detection,
                      CVI_TDL_SUPPORTED_MODEL_PERSON_PETS_DETECTION, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_YOLOV8_Detection, YoloV8Detection,
                      CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_HandFacePerson_Detection, YoloV8Detection,
                      CVI_TDL_SUPPORTED_MODEL_HAND_FACE_PERSON_DETECTION, cvtdl_object_t *)
DEFINE_INF_FUNC_F1_P1(CVI_TDL_HeadPerson_Detection, YoloV8Detection,
                      CVI_TDL_SUPPORTED_MODEL_HEAD_PERSON_DETECTION, cvtdl_object_t *)

CVI_S32 CVI_TDL_CropImage(VIDEO_FRAME_INFO_S *srcFrame, cvtdl_image_t *dst, cvtdl_bbox_t *bbox,
                          bool cvtRGB888) {
  return crop_image(srcFrame, dst, bbox, cvtRGB888);
}

CVI_S32 CVI_TDL_CropImage_Exten(VIDEO_FRAME_INFO_S *srcFrame, cvtdl_image_t *dst,
                                cvtdl_bbox_t *bbox, bool cvtRGB888, float exten_ratio,
                                float *offset_x, float *offset_y) {
  return crop_image_exten(srcFrame, dst, bbox, cvtRGB888, exten_ratio, offset_x, offset_y);
}

CVI_S32 CVI_TDL_CropImage_Face(VIDEO_FRAME_INFO_S *srcFrame, cvtdl_image_t *dst,
                               cvtdl_face_info_t *face_info, bool align, bool cvtRGB888) {
  return crop_image_face(srcFrame, dst, face_info, align, cvtRGB888);
}

// Tracker

CVI_S32 CVI_TDL_DeepSORT_Init(const cvitdl_handle_t handle, bool use_specific_counter) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  if (ds_tracker == nullptr) {
    LOGI("Init DeepSORT.\n");
    ctx->ds_tracker = new DeepSORT(use_specific_counter);
  } else {
    delete ds_tracker;
    LOGI("Re-init DeepSORT.\n");
    ctx->ds_tracker = new DeepSORT(use_specific_counter);
  }
  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_DeepSORT_GetDefaultConfig(cvtdl_deepsort_config_t *ds_conf) {
  cvtdl_deepsort_config_t default_conf = DeepSORT::get_DefaultConfig();
  memcpy(ds_conf, &default_conf, sizeof(cvtdl_deepsort_config_t));

  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_DeepSORT_GetConfig(const cvitdl_handle_t handle, cvtdl_deepsort_config_t *ds_conf,
                                   int cvitdl_obj_type) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_TDL_FAILURE;
  }
  return ds_tracker->getConfig(ds_conf, cvitdl_obj_type);
}

CVI_S32 CVI_TDL_DeepSORT_SetConfig(const cvitdl_handle_t handle, cvtdl_deepsort_config_t *ds_conf,
                                   int cvitdl_obj_type, bool show_config) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_TDL_FAILURE;
  }
  return ds_tracker->setConfig(ds_conf, cvitdl_obj_type, show_config);
}

CVI_S32 CVI_TDL_DeepSORT_CleanCounter(const cvitdl_handle_t handle) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_TDL_FAILURE;
  }
  ds_tracker->cleanCounter();

  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_DeepSORT_Head_FusePed(const cvitdl_handle_t handle, cvtdl_object_t *obj,
                                      cvtdl_tracker_t *tracker_t, bool use_reid,
                                      cvtdl_object_t *head, cvtdl_object_t *ped,
                                      const cvtdl_counting_line_t *counting_line_t,
                                      const randomRect *rect) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  ds_tracker->set_image_size(obj->width, obj->height);
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_FAILURE;
  }
  ctx->ds_tracker->track_headfuse(obj, tracker_t, use_reid, head, ped, counting_line_t, rect);
  return CVI_SUCCESS;
}
CVI_S32 CVI_TDL_DeepSORT_Obj(const cvitdl_handle_t handle, cvtdl_object_t *obj,
                             cvtdl_tracker_t *tracker, bool use_reid) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_TDL_FAILURE;
  }
  return ctx->ds_tracker->track(obj, tracker, use_reid);
}

CVI_S32 CVI_TDL_DeepSORT_Byte(const cvitdl_handle_t handle, cvtdl_object_t *obj,
                              cvtdl_tracker_t *tracker, bool use_reid) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_TDL_FAILURE;
  }
  return ctx->ds_tracker->byte_track(obj, tracker, use_reid);
}
DLL_EXPORT CVI_S32 CVI_TDL_DeepSORT_Obj_Cross(const cvitdl_handle_t handle, cvtdl_object_t *obj,
                                              cvtdl_tracker_t *tracker, bool use_reid,
                                              const cvtdl_counting_line_t *cross_line_t,
                                              const randomRect *rect) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_TDL_FAILURE;
  }
  return ctx->ds_tracker->track_cross(obj, tracker, use_reid, cross_line_t, rect);
}

CVI_S32 CVI_TDL_DeepSORT_Face(const cvitdl_handle_t handle, cvtdl_face_t *face,
                              cvtdl_tracker_t *tracker) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_TDL_FAILURE;
  }
  return ctx->ds_tracker->track(face, tracker);
}
DLL_EXPORT CVI_S32 CVI_TDL_DeepSORT_FaceFusePed(const cvitdl_handle_t handle, cvtdl_face_t *face,
                                                cvtdl_object_t *obj, cvtdl_tracker_t *tracker_t) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  ds_tracker->set_image_size(face->width, face->height);
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_FAILURE;
  }
  ctx->ds_tracker->track_fuse(obj, face, tracker_t);
  return CVI_SUCCESS;
}

CVI_S32 CVI_TDL_DeepSORT_Set_Timestamp(const cvitdl_handle_t handle, uint32_t ts) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_FAILURE;
  }
  ctx->ds_tracker->set_timestamp(ts);
  return CVI_SUCCESS;
}

CVI_S32 CVI_TDL_DeepSORT_UpdateOutNum(const cvitdl_handle_t handle, cvtdl_tracker_t *tracker_t) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;

  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_FAILURE;
  }
  ctx->ds_tracker->update_out_num(tracker_t);
  return CVI_SUCCESS;
}
CVI_S32 CVI_TDL_DeepSORT_DebugInfo_1(const cvitdl_handle_t handle, char *debug_info) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_TDL_FAILURE;
  }
  std::string str_info;
  ctx->ds_tracker->get_TrackersInfo_UnmatchedLastTime(str_info);
  strncpy(debug_info, str_info.c_str(), 8192);

  return CVI_TDL_SUCCESS;
}

CVI_S32 CVI_TDL_DeepSORT_GetTracker_Inactive(const cvitdl_handle_t handle,
                                             cvtdl_tracker_t *tracker) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  DeepSORT *ds_tracker = ctx->ds_tracker;
  if (ds_tracker == nullptr) {
    LOGE("Please initialize DeepSORT first.\n");
    return CVI_FAILURE;
  }
  return ctx->ds_tracker->get_trackers_inactive(tracker);
}

// Fall Detection

CVI_S32 CVI_TDL_Fall(const cvitdl_handle_t handle, cvtdl_object_t *objects) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  FallMD *fall_model = ctx->fall_model;
  if (fall_model == nullptr) {
    LOGD("Init Fall Detection Model.\n");
    ctx->fall_model = new FallMD();
    ctx->fall_model->detect(objects);
    return CVI_TDL_SUCCESS;
  }
  return ctx->fall_model->detect(objects);
}

// New Fall Detection

CVI_S32 CVI_TDL_Fall_Monitor(const cvitdl_handle_t handle, cvtdl_object_t *objects) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  FallDetMonitor *fall_monitor_model = ctx->fall_monitor_model;
  if (fall_monitor_model == nullptr) {
    LOGD("Init Fall Detection Model.\n");
    ctx->fall_monitor_model = new FallDetMonitor();
    ctx->fall_monitor_model->monitor(objects);
    return CVI_TDL_SUCCESS;
  }
  return ctx->fall_monitor_model->monitor(objects);
}

CVI_S32 CVI_TDL_Set_Fall_FPS(const cvitdl_handle_t handle, float fps) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  FallDetMonitor *fall_monitor_model = ctx->fall_monitor_model;
  if (fall_monitor_model == nullptr) {
    LOGD("Init Fall Detection Model.\n");
    ctx->fall_monitor_model = new FallDetMonitor();
    ctx->fall_monitor_model->set_fps(fps);
    return CVI_TDL_SUCCESS;
  }
  return ctx->fall_monitor_model->set_fps(fps);
}

// Others
CVI_S32 CVI_TDL_TamperDetection(const cvitdl_handle_t handle, VIDEO_FRAME_INFO_S *frame,
                                float *moving_score) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  TamperDetectorMD *td_model = ctx->td_model;
  if (td_model == nullptr) {
    LOGD("Init Tamper Detection Model.\n");
    createIVEHandleIfNeeded(ctx);
    ctx->td_model = new TamperDetectorMD(ctx->ive_handle, frame, (float)0.05, (int)10);

    *moving_score = -1.0;
    return CVI_TDL_SUCCESS;
  }
  return ctx->td_model->detect(frame, moving_score);
}

CVI_S32 CVI_TDL_Set_MotionDetection_Background(const cvitdl_handle_t handle,
                                               VIDEO_FRAME_INFO_S *frame) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  MotionDetection *md_model = ctx->md_model;
  if (md_model == nullptr) {
    LOGD("Init Motion Detection.\n");
    if (createIVEHandleIfNeeded(ctx) == CVI_TDL_FAILURE) {
      return CVI_TDL_FAILURE;
    }
    ctx->md_model = new MotionDetection(ctx->ive_handle);
    return ctx->md_model->init(frame);
  }
  return ctx->md_model->update_background(frame);
}

CVI_S32 CVI_TDL_Set_MotionDetection_ROI(const cvitdl_handle_t handle, MDROI_t *roi_s) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  MotionDetection *md_model = ctx->md_model;
  if (md_model == nullptr) {
    LOGE("MD has not been inited\n");
    return CVI_TDL_FAILURE;
  }
  return ctx->md_model->set_roi(roi_s);
}

CVI_S32 CVI_TDL_MotionDetection(const cvitdl_handle_t handle, VIDEO_FRAME_INFO_S *frame,
                                cvtdl_object_t *obj_meta, uint8_t threshold, double min_area) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  MotionDetection *md_model = ctx->md_model;
  if (md_model == nullptr) {
    LOGE(
        "Failed to do motion detection! Please invoke CVI_TDL_Set_MotionDetection_Background to "
        "set "
        "background image first.\n");
    return CVI_TDL_FAILURE;
  }
  std::vector<std::vector<float>> boxes;
  CVI_S32 ret = ctx->md_model->detect(frame, boxes, threshold, min_area);
  memset(obj_meta, 0, sizeof(cvtdl_object_t));
  size_t num_boxes = boxes.size();
  if (num_boxes > 0) {
    CVI_TDL_MemAllocInit(num_boxes, obj_meta);
    obj_meta->height = frame->stVFrame.u32Height;
    obj_meta->width = frame->stVFrame.u32Width;
    obj_meta->rescale_type = RESCALE_RB;
    memset(obj_meta->info, 0, sizeof(cvtdl_object_info_t) * num_boxes);
    for (uint32_t i = 0; i < (uint32_t)num_boxes; ++i) {
      obj_meta->info[i].bbox.x1 = boxes[i][0];
      obj_meta->info[i].bbox.y1 = boxes[i][1];
      obj_meta->info[i].bbox.x2 = boxes[i][2];
      obj_meta->info[i].bbox.y2 = boxes[i][3];
      obj_meta->info[i].bbox.score = 0;
      obj_meta->info[i].classes = -1;
      memset(obj_meta->info[i].name, 0, sizeof(obj_meta->info[i].name));
    }
  }
  return ret;
}

CVI_S32 CVI_TDL_FaceFeatureExtract(const cvitdl_handle_t handle, const uint8_t *p_rgb_pack,
                                   int width, int height, int stride,
                                   cvtdl_face_info_t *p_face_info) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  FaceAttribute *inst = dynamic_cast<FaceAttribute *>(
      getInferenceInstance(CVI_TDL_SUPPORTED_MODEL_FACERECOGNITION, ctx));
  if (inst == nullptr) {
    LOGE("No instance found for FaceAttribute\n");
    return CVI_FAILURE;
  }
  if (inst->isInitialized()) {
    if (initVPSSIfNeeded(ctx, CVI_TDL_SUPPORTED_MODEL_FACERECOGNITION) != CVI_SUCCESS) {
      return CVI_TDL_ERR_INIT_VPSS;
    }
  } else {
    LOGE("Model (%s)is not yet opened! Please call CVI_TDL_OpenModel to initialize model\n",
         CVI_TDL_GetModelName(CVI_TDL_SUPPORTED_MODEL_FACERECOGNITION));
    return CVI_TDL_ERR_NOT_YET_INITIALIZED;
  }
  return inst->extract_face_feature(p_rgb_pack, width, height, stride, p_face_info);
}

CVI_S32 CVI_TDL_Get_SoundClassification_ClassesNum(const cvitdl_handle_t handle) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  SoundClassification *sc_model = dynamic_cast<SoundClassification *>(
      getInferenceInstance(CVI_TDL_SUPPORTED_MODEL_SOUNDCLASSIFICATION, ctx));
  if (sc_model == nullptr) {
    LOGE("No instance found for SoundClassification.\n");
    return CVI_TDL_ERR_OPEN_MODEL;
  }
  if (sc_model->isInitialized()) {
    return sc_model->getClassesNum();
  } else {
    LOGE("Model (%s)is not yet opened! Please call CVI_TDL_OpenModel to initialize model\n",
         CVI_TDL_GetModelName(CVI_TDL_SUPPORTED_MODEL_SOUNDCLASSIFICATION));
    return CVI_TDL_ERR_NOT_YET_INITIALIZED;
  }
}

CVI_S32 CVI_TDL_Set_SoundClassification_Threshold(const cvitdl_handle_t handle, const float th) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  SoundClassification *sc_model = dynamic_cast<SoundClassification *>(
      getInferenceInstance(CVI_TDL_SUPPORTED_MODEL_SOUNDCLASSIFICATION, ctx));
  if (sc_model == nullptr) {
    LOGE("No instance found for SoundClassification.\n");
    return CVI_TDL_ERR_OPEN_MODEL;
  }
  if (sc_model->isInitialized()) {
    return sc_model->setThreshold(th);
  } else {
    LOGE("Model (%s)is not yet opened! Please call CVI_TDL_OpenModel to initialize model\n",
         CVI_TDL_GetModelName(CVI_TDL_SUPPORTED_MODEL_SOUNDCLASSIFICATION));
    return CVI_TDL_ERR_NOT_YET_INITIALIZED;
  }
}

CVI_S32 CVI_TDL_Change_Img(const cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E model_type,
                           VIDEO_FRAME_INFO_S *frame, VIDEO_FRAME_INFO_S **dst_frame,
                           PIXEL_FORMAT_E enDstFormat) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  cvitdl_model_t modelt = ctx->model_cont[model_type];
  if (modelt.instance == nullptr) {
    LOGE("model not initialized:%d\n", (int)model_type);
    return CVI_FAILURE;
  }

  VpssEngine *p_vpss_inst = modelt.instance->get_vpss_instance();
  if (p_vpss_inst == nullptr) {
    LOGE("vpssmodel not initialized:%d\n", (int)model_type);
    return CVI_FAILURE;
  }

  VIDEO_FRAME_INFO_S *f = new VIDEO_FRAME_INFO_S;
  memset(f, 0, sizeof(VIDEO_FRAME_INFO_S));
  modelt.instance->vpssChangeImage(frame, f, frame->stVFrame.u32Width, frame->stVFrame.u32Height,
                                   enDstFormat);
  *dst_frame = f;
  return CVI_SUCCESS;
}

CVI_S32 CVI_TDL_Delete_Img(const cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E model_type,
                           VIDEO_FRAME_INFO_S *p_f) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  cvitdl_model_t modelt = ctx->model_cont[model_type];
  if (modelt.instance == nullptr) {
    LOGE("model not initialized:%d\n", (int)model_type);
    return CVI_FAILURE;
  }
  VpssEngine *p_vpss_inst = modelt.instance->get_vpss_instance();

  if (p_vpss_inst == nullptr) {
    LOGE("vpssmodel not initialized:%d\n", (int)model_type);
    return CVI_FAILURE;
  }
  p_vpss_inst->releaseFrame(p_f, 0);
  delete p_f;
  return CVI_SUCCESS;
}

CVI_S32 CVI_TDL_CropImage_With_VPSS(const cvitdl_handle_t handle,
                                    CVI_TDL_SUPPORTED_MODEL_E model_type, VIDEO_FRAME_INFO_S *frame,
                                    const cvtdl_bbox_t *p_crop_box, cvtdl_image_t *p_dst) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  cvitdl_model_t modelt = ctx->model_cont[model_type];
  if (modelt.instance == nullptr) {
    LOGE("model not initialized:%d\n", (int)model_type);
    return CVI_FAILURE;
  }
  VpssEngine *p_vpss_inst = modelt.instance->get_vpss_instance();

  if (p_vpss_inst == nullptr) {
    LOGE("vpssmodel not initialized:%d\n", (int)model_type);
    return CVI_TDL_ERR_NOT_YET_INITIALIZED;
  }
  if (p_dst->pix_format != PIXEL_FORMAT_RGB_888) {
    LOGE("only PIXEL_FORMAT_RGB_888 format supported,got:%d\n", (int)p_dst->pix_format);
    return CVI_FAILURE;
  }

  VIDEO_FRAME_INFO_S *f = new VIDEO_FRAME_INFO_S;
  memset(f, 0, sizeof(VIDEO_FRAME_INFO_S));
  modelt.instance->vpssCropImage(frame, f, *p_crop_box, p_dst->width, p_dst->height,
                                 p_dst->pix_format);
  mmap_video_frame(f);

  int ret = CVI_SUCCESS;
  for (int i = 0; i < 3; i++) {
    if ((p_dst->pix[i] == 0 && f->stVFrame.pu8VirAddr[i] != 0) ||
        (p_dst->pix[i] != 0 && f->stVFrame.pu8VirAddr[i] == 0)) {
      LOGE("error,plane:%d,dst_addr:%p,video_frame_addr:%p", i, p_dst->pix[i],
           f->stVFrame.pu8VirAddr[i]);
      ret = CVI_FAILURE;
      break;
    }
    if (f->stVFrame.u32Length[i] > p_dst->length[i]) {
      LOGE("size overflow,plane:%d,dst_len:%u,video_frame_len:%u", i, p_dst->length[i],
           f->stVFrame.u32Length[i]);
      ret = CVI_FAILURE;
      break;
    }
    memcpy(p_dst->pix[i], f->stVFrame.pu8VirAddr[i], f->stVFrame.u32Length[i]);
  }
  unmap_video_frame(f);
  if (f->stVFrame.u64PhyAddr[0] != 0) {
    p_vpss_inst->releaseFrame(f, 0);
  }
  delete f;
  return ret;
}
CVI_S32 CVI_TDL_CropResizeImage(const cvitdl_handle_t handle, CVI_TDL_SUPPORTED_MODEL_E model_type,
                                VIDEO_FRAME_INFO_S *frame, const cvtdl_bbox_t *p_crop_box,
                                int dst_width, int dst_height, PIXEL_FORMAT_E enDstFormat,
                                VIDEO_FRAME_INFO_S **p_dst_img) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  cvitdl_model_t modelt = ctx->model_cont[model_type];
  if (modelt.instance == nullptr) {
    LOGE("model not initialized:%d\n", (int)model_type);
    return CVI_FAILURE;
  }
  VpssEngine *p_vpss_inst = modelt.instance->get_vpss_instance();

  if (p_vpss_inst == nullptr) {
    LOGE("vpssmodel not initialized:%d\n", (int)model_type);
    return CVI_TDL_ERR_NOT_YET_INITIALIZED;
  }

  VIDEO_FRAME_INFO_S *f = new VIDEO_FRAME_INFO_S;
  memset(f, 0, sizeof(VIDEO_FRAME_INFO_S));
  int ret =
      modelt.instance->vpssCropImage(frame, f, *p_crop_box, dst_width, dst_height, enDstFormat);
  *p_dst_img = f;
  return ret;
}
CVI_S32 CVI_TDL_Copy_VideoFrameToImage(VIDEO_FRAME_INFO_S *f, cvtdl_image_t *p_dst) {
  mmap_video_frame(f);

  int ret = CVI_SUCCESS;
  for (int i = 0; i < 3; i++) {
    if ((p_dst->pix[i] == 0 && f->stVFrame.pu8VirAddr[i] != 0) ||
        (p_dst->pix[i] != 0 && f->stVFrame.pu8VirAddr[i] == 0)) {
      LOGE("error,plane:%d,dst_addr:%p,video_frame_addr:%p", i, p_dst->pix[i],
           f->stVFrame.pu8VirAddr[i]);
      ret = CVI_FAILURE;
      break;
    }
    if (f->stVFrame.u32Length[i] > p_dst->length[i]) {
      LOGE("size overflow,plane:%d,dst_len:%u,video_frame_len:%u", i, p_dst->length[i],
           f->stVFrame.u32Length[i]);
      ret = CVI_FAILURE;
      break;
    }
    memcpy(p_dst->pix[i], f->stVFrame.pu8VirAddr[i], f->stVFrame.u32Length[i]);
  }
  unmap_video_frame(f);
  return ret;
}
CVI_S32 CVI_TDL_Resize_VideoFrame(const cvitdl_handle_t handle,
                                  CVI_TDL_SUPPORTED_MODEL_E model_type, VIDEO_FRAME_INFO_S *frame,
                                  const int dst_w, const int dst_h, PIXEL_FORMAT_E dst_format,
                                  VIDEO_FRAME_INFO_S **dst_frame) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  cvitdl_model_t modelt = ctx->model_cont[model_type];
  if (modelt.instance == nullptr) {
    LOGE("model not initialized:%d\n", (int)model_type);
    return CVI_FAILURE;
  }
  VpssEngine *p_vpss_inst = modelt.instance->get_vpss_instance();

  if (p_vpss_inst == nullptr) {
    LOGE("vpssmodel not initialized:%d\n", (int)model_type);
    return CVI_TDL_ERR_NOT_YET_INITIALIZED;
  }

  VIDEO_FRAME_INFO_S *f = new VIDEO_FRAME_INFO_S;
  memset(f, 0, sizeof(VIDEO_FRAME_INFO_S));
  cvtdl_bbox_t bbox;
  bbox.x1 = 0;
  bbox.y1 = 0;
  bbox.x2 = frame->stVFrame.u32Width;
  bbox.y2 = frame->stVFrame.u32Height;
  VPSS_SCALE_COEF_E scale = VPSS_SCALE_COEF_NEAREST;
  modelt.instance->vpssCropImage(frame, f, bbox, dst_w, dst_h, dst_format, scale);
  *dst_frame = f;
  return CVI_SUCCESS;
}
DLL_EXPORT CVI_S32 CVI_TDL_Release_VideoFrame(const cvitdl_handle_t handle,
                                              CVI_TDL_SUPPORTED_MODEL_E model_type,
                                              VIDEO_FRAME_INFO_S *frame, bool del_frame) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  cvitdl_model_t modelt = ctx->model_cont[model_type];
  if (modelt.instance == nullptr) {
    LOGE("model not initialized:%d\n", (int)model_type);
    return CVI_FAILURE;
  }
  VpssEngine *p_vpss_inst = modelt.instance->get_vpss_instance();

  if (p_vpss_inst == nullptr) {
    LOGE("vpssmodel not initialized:%d\n", (int)model_type);
    return CVI_TDL_ERR_NOT_YET_INITIALIZED;
  }

  if (frame->stVFrame.u64PhyAddr[0] != 0) {
    p_vpss_inst->releaseFrame(frame, 0);
  }
  if (del_frame) {
    delete frame;
  }
  return CVI_SUCCESS;
}

CVI_S32 CVI_TDL_PersonVehicle_Detection(const cvitdl_handle_t handle, VIDEO_FRAME_INFO_S *frame,
                                        cvtdl_object_t *obj_meta) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  YoloV8Detection *yolo_model = dynamic_cast<YoloV8Detection *>(
      getInferenceInstance(CVI_TDL_SUPPORTED_MODEL_PERSON_VEHICLE_DETECTION, ctx));
  if (yolo_model == nullptr) {
    LOGE("No instance found for CVI_TDL_Hand_Detection.\n");
    return CVI_FAILURE;
  }
  LOGI("got yolov8 instance\n");
  if (yolo_model->isInitialized()) {
    if (initVPSSIfNeeded(ctx, CVI_TDL_SUPPORTED_MODEL_PERSON_VEHICLE_DETECTION) != CVI_SUCCESS) {
      return CVI_TDL_ERR_INIT_VPSS;
    } else {
      int ret = yolo_model->inference(frame, obj_meta);
      ret = yolo_model->after_inference();

      if (ret == CVI_TDL_SUCCESS) {
        for (uint32_t i = 0; i < obj_meta->size; i++) {
          if (obj_meta->info[i].classes == 4) {
            obj_meta->info[i].classes = 0;  // person
          } else if (obj_meta->info[i].classes == 0 || obj_meta->info[i].classes == 1 ||
                     obj_meta->info[i].classes == 2) {
            obj_meta->info[i].classes = 1;  // motor vehicle
          } else {
            obj_meta->info[i].classes = 2;  // non-motor vehicle
          }
        }
      }
      return ret;
    }
  } else {
    LOGE("Model (%s)is not yet opened! Please call CVI_TDL_OpenModel to initialize model\n",
         CVI_TDL_GetModelName(CVI_TDL_SUPPORTED_MODEL_PERSON_VEHICLE_DETECTION));
    return CVI_TDL_ERR_NOT_YET_INITIALIZED;
  }
}

CVI_S32 CVI_TDL_Set_Yolov5_ROI(const cvitdl_handle_t handle, Point_t roi_s) {
  printf("enter CVI_TDL_Set_Yolov5_ROI...\n");
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  Yolov5 *yolov5_model =
      dynamic_cast<Yolov5 *>(getInferenceInstance(CVI_TDL_SUPPORTED_MODEL_YOLOV5, ctx));
  if (yolov5_model == nullptr) {
    LOGE("yolov5_model has not been inited\n");
    return CVI_TDL_FAILURE;
  }
  return yolov5_model->set_roi(roi_s);
}

YoloPreParam CVI_TDL_Get_YOLO_Preparam(const cvitdl_handle_t handle,
                                       const CVI_TDL_SUPPORTED_MODEL_E model_index) {
  printf("enter CVI_TDL_Get_YOLO_Preparam...\n");
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);

  if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV5 ||
      model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV7) {
    Yolov5 *yolov5_model = dynamic_cast<Yolov5 *>(getInferenceInstance(model_index, ctx));
    return yolov5_model->get_preparam();
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV6) {
    Yolov6 *yolov6_model = dynamic_cast<Yolov6 *>(getInferenceInstance(model_index, ctx));
    return yolov6_model->get_preparam();
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION) {
    YoloV8Detection *yolov8_model =
        dynamic_cast<YoloV8Detection *>(getInferenceInstance(model_index, ctx));
    return yolov8_model->get_preparam();
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOX) {
    YoloX *yolox_model = dynamic_cast<YoloX *>(getInferenceInstance(model_index, ctx));
    return yolox_model->get_preparam();
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_PPYOLOE) {
    PPYoloE *ppyoloe_model = dynamic_cast<PPYoloE *>(getInferenceInstance(model_index, ctx));
    return ppyoloe_model->get_preparam();
  }

  return YoloPreParam();
}

CVI_S32 CVI_TDL_Set_YOLO_Preparam(const cvitdl_handle_t handle,
                                  const CVI_TDL_SUPPORTED_MODEL_E model_index,
                                  YoloPreParam pre_param) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);

  if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV5 ||
      model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV7) {
    Yolov5 *yolov5_model = dynamic_cast<Yolov5 *>(getInferenceInstance(model_index, ctx));
    yolov5_model->set_preparam(pre_param);
    return CVI_SUCCESS;
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV6) {
    Yolov6 *yolov6_model = dynamic_cast<Yolov6 *>(getInferenceInstance(model_index, ctx));
    yolov6_model->set_preparam(pre_param);
    return CVI_SUCCESS;
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION) {
    YoloV8Detection *yolov8_model =
        dynamic_cast<YoloV8Detection *>(getInferenceInstance(model_index, ctx));
    yolov8_model->set_preparam(pre_param);
    return CVI_SUCCESS;
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOX) {
    YoloX *yolox_model = dynamic_cast<YoloX *>(getInferenceInstance(model_index, ctx));
    yolox_model->set_preparam(pre_param);
    return CVI_SUCCESS;
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_PPYOLOE) {
    PPYoloE *ppyoloe_model = dynamic_cast<PPYoloE *>(getInferenceInstance(model_index, ctx));
    ppyoloe_model->set_preparam(pre_param);
    return CVI_SUCCESS;
  }
  LOGE("not supported model index\n");
  return CVI_FAILURE;
}

// TODO remove model_index
YoloAlgParam CVI_TDL_Get_YOLO_Algparam(const cvitdl_handle_t handle,
                                       const CVI_TDL_SUPPORTED_MODEL_E model_index) {
  printf("enter CVI_TDL_Get_YOLO_Preparam...\n");
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV5 ||
      model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV7) {
    Yolov5 *yolov5_model = dynamic_cast<Yolov5 *>(getInferenceInstance(model_index, ctx));
    return yolov5_model->get_algparam();
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV6) {
    Yolov6 *yolov6_model = dynamic_cast<Yolov6 *>(getInferenceInstance(model_index, ctx));
    return yolov6_model->get_algparam();
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION) {
    YoloV8Detection *yolov8_model =
        dynamic_cast<YoloV8Detection *>(getInferenceInstance(model_index, ctx));
    return yolov8_model->get_algparam();
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOX) {
    YoloX *yolox_model = dynamic_cast<YoloX *>(getInferenceInstance(model_index, ctx));
    return yolox_model->get_algparam();
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_PPYOLOE) {
    PPYoloE *ppyoloe_model = dynamic_cast<PPYoloE *>(getInferenceInstance(model_index, ctx));
    return ppyoloe_model->get_algparam();
  }
  return YoloAlgParam();
}

CVI_S32 CVI_TDL_Set_YOLO_Algparam(const cvitdl_handle_t handle,
                                  const CVI_TDL_SUPPORTED_MODEL_E model_index,
                                  YoloAlgParam alg_param) {
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);

  if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV5 ||
      model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV7) {
    Yolov5 *yolov5_model = dynamic_cast<Yolov5 *>(getInferenceInstance(model_index, ctx));
    yolov5_model->set_algparam(alg_param);
    return CVI_SUCCESS;
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV6) {
    Yolov6 *yolov6_model = dynamic_cast<Yolov6 *>(getInferenceInstance(model_index, ctx));
    yolov6_model->set_algparam(alg_param);
    return CVI_SUCCESS;
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION) {
    YoloV8Detection *yolov8_model =
        dynamic_cast<YoloV8Detection *>(getInferenceInstance(model_index, ctx));
    yolov8_model->set_algparam(alg_param);
    return CVI_SUCCESS;
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_YOLOX) {
    YoloX *yolox_model = dynamic_cast<YoloX *>(getInferenceInstance(model_index, ctx));
    yolox_model->set_algparam(alg_param);
    return CVI_SUCCESS;
  } else if (model_index == CVI_TDL_SUPPORTED_MODEL_PPYOLOE) {
    PPYoloE *ppyoloe_model = dynamic_cast<PPYoloE *>(getInferenceInstance(model_index, ctx));
    ppyoloe_model->set_algparam(alg_param);
    return CVI_SUCCESS;
  }
  LOGE("not supported model index\n");
  return CVI_FAILURE;
}

CVI_S32 CVI_TDL_Set_Image_Cls_Param(const cvitdl_handle_t handle, VpssPreParam *p_preprocess_cfg) {
  printf("enter CVI_TDL_Set_Image_Classification_Param...\n");
  cvitdl_context_t *ctx = static_cast<cvitdl_context_t *>(handle);
  ImageClassification *image_cls_model = dynamic_cast<ImageClassification *>(
      getInferenceInstance(CVI_TDL_SUPPORTED_MODEL_IMAGE_CLASSIFICATION, ctx));
  if (image_cls_model == nullptr) {
    LOGE("No instance found for image classification.\n");
    return CVI_FAILURE;
  }
  LOGI("got image_cls_model instance\n");
  if (p_preprocess_cfg == nullptr) {
    LOGE("p_preprocess_cfg can not be nullptr.\n");
    return CVI_FAILURE;
  }

  image_cls_model->set_param(p_preprocess_cfg);
  return CVI_SUCCESS;
}
