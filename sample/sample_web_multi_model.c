#define LOG_TAG "SampleOD"
#define LOG_LEVEL LOG_LEVEL_INFO

#include "middleware_utils.h"
#include "sample_log.h"
#include "sample_utils.h"
#include "vi_vo_utils.h"

#include <core/utils/vpss_helper.h>
#include <cvi_comm.h>
#include <cvi_sys.h>
#include <cvi_vb.h>
#include <cvi_vi.h>
#include <cviai.h>
#include <rtsp.h>
#include <sample_comm.h>

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ai_type.h"
#include "app_ipcam_netctrl.h"
#include "app_ipcam_websocket.h"

static volatile bool bExit = false;
static cvai_object_t g_stObjMeta;
MUTEXAUTOLOCK_INIT(ResultMutex);

static SAMPLE_AI_TYPE g_ai_type = 0;
int clrs[7][3] = {{0, 0, 255},   {0, 255, 0},   {255, 0, 0}, {0, 255, 255},
                  {255, 0, 255}, {255, 255, 0}, {0, 0, 0}};

cvai_service_brush_t *get_obj_brush(cvai_object_t *p_obj_meta) {
  cvai_service_brush_t *p_brush = (cvai_object_t *)malloc(p_obj_meta->size * sizeof(cvai_object_t));
  const int max_clr = 7;
  for (size_t i = 0; i < p_obj_meta->size; i++) {
    int clr_idx = p_obj_meta->info[i].classes % max_clr;
    printf("obj cls:%d\n", p_obj_meta->info[i].classes);
    p_brush[i].color.b = clrs[clr_idx][0];
    p_brush[i].color.g = clrs[clr_idx][1];
    p_brush[i].color.r = clrs[clr_idx][2];
    p_brush[i].size = 4;
  }
  return p_brush;
}
SAMPLE_AI_TYPE *ai_param_get(void) { return &g_ai_type; }

void ai_param_set(SAMPLE_AI_TYPE ai_type) { g_ai_type = ai_type; }

int init_func(cviai_handle_t ai_handle, SAMPLE_AI_TYPE algo_type) {
  int ret = 0;
  if (algo_type == CVI_AI_HAND) {
    const char *det_path = "/mnt/data/models/hand.cvimodel";
    ret = CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_HAND_DETECTION, det_path);
    if (ret != CVI_SUCCESS) {
      printf("failed to open face reg model %s\n", det_path);
      return ret;
    }
    CVI_AI_SetModelThreshold(ai_handle, CVI_AI_SUPPORTED_MODEL_HAND_DETECTION, 0.4);

    const char *cls_model_path = "/mnt/data/models/hand_cls_int8_cv182x.cvimodel";

    ret = CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_HANDCLASSIFICATION, cls_model_path);
    if (ret != CVI_SUCCESS) {
      printf("failed to open face reg model %s\n", cls_model_path);
      return ret;
    }
  } else if (algo_type == CVI_AI_OBJECT) {
    const char *det_path = "/mnt/data/models//mobiledetv2-pedestrian-d0-ls-384.cvimodel";
    ret = CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN, det_path);
  } else if (algo_type == CVI_AI_PET) {
    const char *det_path = "/mnt/data/models/pet.cvimodel";
    ret = CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_PERSON_PETS_DETECTION, det_path);
  } else if (algo_type == CVI_AI_PERSON_VEHICLE) {
    const char *det_path = "/mnt/data/models/person_vehicle.cvimodel";
    ret = CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_PERSON_VEHICLE_DETECTION, det_path);
  } else if (algo_type == CVI_AI_MEET) {
    const char *det_path = "/mnt/data/models/meet.cvimodel";
    ret = CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_HAND_FACE_PERSON_DETECTION, det_path);
    const char *cls_model_path = "/mnt/data/models/hand_cls_int8_cv182x.cvimodel";
    ret = CVI_AI_OpenModel(ai_handle, CVI_AI_SUPPORTED_MODEL_HANDCLASSIFICATION, cls_model_path);
    if (ret != CVI_SUCCESS) {
      printf("failed to open face reg model %s\n", cls_model_path);
      return ret;
    }
  }

  return ret;
}
int release_func(cviai_handle_t ai_handle, SAMPLE_AI_TYPE algo_type) {
  int ret = 0;
  if (algo_type == CVI_AI_HAND) {
    CVI_AI_CloseModel(ai_handle, CVI_AI_SUPPORTED_MODEL_HAND_DETECTION);
    CVI_AI_CloseModel(ai_handle, CVI_AI_SUPPORTED_MODEL_HANDCLASSIFICATION);
  } else if (algo_type == CVI_AI_OBJECT) {
    CVI_AI_CloseModel(ai_handle, CVI_AI_SUPPORTED_MODEL_MOBILEDETV2_PEDESTRIAN);
  } else if (algo_type == CVI_AI_PET) {
    CVI_AI_CloseModel(ai_handle, CVI_AI_SUPPORTED_MODEL_PERSON_PETS_DETECTION);
  } else if (algo_type == CVI_AI_PERSON_VEHICLE) {
    CVI_AI_CloseModel(ai_handle, CVI_AI_SUPPORTED_MODEL_PERSON_VEHICLE_DETECTION);
  } else if (algo_type == CVI_AI_MEET) {
    CVI_AI_CloseModel(ai_handle, CVI_AI_SUPPORTED_MODEL_HAND_FACE_PERSON_DETECTION);
    CVI_AI_CloseModel(ai_handle, CVI_AI_SUPPORTED_MODEL_HANDCLASSIFICATION);
  }

  return ret;
}
int infer_func(cviai_handle_t ai_handle, SAMPLE_AI_TYPE algo_type, VIDEO_FRAME_INFO_S *ptr_frame,
               cvai_object_t *p_stObjMeta) {
  int ret = 0;
  if (algo_type == CVI_AI_HAND) {
    ret = CVI_AI_Hand_Detection(ai_handle, ptr_frame, p_stObjMeta);
    if (ret != CVIAI_SUCCESS) {
      AI_LOGE("inference failed!, ret=%x\n", ret);
      return ret;
    }
    ret = CVI_AI_HandClassification(ai_handle, ptr_frame, p_stObjMeta);
    if (ret != CVIAI_SUCCESS) {
      AI_LOGE("inference failed!, ret=%x\n", ret);
    }
  } else if (algo_type == CVI_AI_OBJECT) {
    ret = CVI_AI_MobileDetV2_Pedestrian(ai_handle, ptr_frame, p_stObjMeta);
  } else if (algo_type == CVI_AI_PET) {
    ret = CVI_AI_PersonPet_Detection(ai_handle, ptr_frame, p_stObjMeta);
  } else if (algo_type == CVI_AI_PERSON_VEHICLE) {
    ret = CVI_AI_PersonVehicle_Detection(ai_handle, ptr_frame, p_stObjMeta);
  } else if (algo_type == CVI_AI_MEET) {
    ret = CVI_AI_HandFacePerson_Detection(ai_handle, ptr_frame, p_stObjMeta);
    ret = CVI_AI_HandClassification(ai_handle, ptr_frame, p_stObjMeta);
  }
  return ret;
}

/**
 * @brief Arguments for video encoder thread
 *
 */
typedef struct {
  SAMPLE_AI_MW_CONTEXT *pstMWContext;
  cviai_service_handle_t stServiceHandle;
} SAMPLE_AI_VENC_THREAD_ARG_S;

typedef struct {
  cviai_handle_t stAiHandle;
} SAMPLE_AI_AI_THREAD_ARG_S;

void *run_venc_thread(void *args) {
  AI_LOGI("Enter encoder thread\n");
  SAMPLE_AI_VENC_THREAD_ARG_S *pstArgs = (SAMPLE_AI_VENC_THREAD_ARG_S *)args;
  VIDEO_FRAME_INFO_S stFrame;
  CVI_S32 s32Ret;
  cvai_object_t stObjMeta = {0};

  while (bExit == false) {
    s32Ret = CVI_VPSS_GetChnFrame(0, VPSS_CHN0, &stFrame, 2000);
    if (s32Ret != CVI_SUCCESS) {
      AI_LOGE("CVI_VPSS_GetChnFrame chn0 failed with %#x\n", s32Ret);
      break;
    }

    char name[256];
    MutexAutoLock(ResultMutex, lock);
    CVI_AI_CopyObjectMeta(&g_stObjMeta, &stObjMeta);
    for (uint32_t oid = 0; oid < stObjMeta.size; oid++) {
      sprintf(name, "%s: %.2f", stObjMeta.info[oid].name, stObjMeta.info[oid].bbox.score);
      memcpy(stObjMeta.info[oid].name, name, sizeof(stObjMeta.info[oid].name));
    }
    if (stObjMeta.size > 0) {
      cvai_service_brush_t *p_brushes = get_obj_brush(&stObjMeta);
      s32Ret = CVI_AI_Service_ObjectDrawRect2(pstArgs->stServiceHandle, &stObjMeta, &stFrame, true,
                                              p_brushes);
      free(p_brushes);
    }

    if (s32Ret != CVIAI_SUCCESS) {
      CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
      AI_LOGE("Draw fame fail!, ret=%x\n", s32Ret);
      goto error;
    }

    s32Ret = SAMPLE_AI_Send_Frame_WEB(&stFrame, pstArgs->pstMWContext);
  error:
    CVI_AI_Free(&stObjMeta);
    CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
    if (s32Ret != CVI_SUCCESS) {
      bExit = true;
    }
  }
  AI_LOGI("Exit encoder thread\n");
  pthread_exit(NULL);
}

void *run_ai_thread(void *args) {
  AI_LOGI("Enter AI thread\n");
  SAMPLE_AI_AI_THREAD_ARG_S *pstAIArgs = (SAMPLE_AI_AI_THREAD_ARG_S *)args;
  VIDEO_FRAME_INFO_S stFrame;

  CVI_S32 s32Ret;
  SAMPLE_AI_TYPE ai_type = CVI_AI_HAND;
  int ret = init_func(pstAIArgs->stAiHandle, CVI_AI_HAND);
  if (ret != 0) {
    goto inf_error;
  }

  while (bExit == false) {
    if (ai_type != g_ai_type && g_ai_type < CVI_AI_MAX) {
      release_func(pstAIArgs->stAiHandle, ai_type);
      init_func(pstAIArgs->stAiHandle, g_ai_type);
      ai_type = g_ai_type;
    }
    s32Ret = CVI_VPSS_GetChnFrame(0, VPSS_CHN1, &stFrame, 2000);

    if (s32Ret != CVI_SUCCESS) {
      AI_LOGE("CVI_VPSS_GetChnFrame failed with %#x\n", s32Ret);
      goto get_frame_failed;
    }

    struct timeval t0, t1;
    gettimeofday(&t0, NULL);

    cvai_object_t stObjMeta = {0};
    ret = infer_func(pstAIArgs->stAiHandle, ai_type, &stFrame, &stObjMeta);
    if (ret != 0) {
      goto inf_error;
    }
    CVI_AI_CopyObjectMeta(&stObjMeta, &g_stObjMeta);
    CVI_AI_Free(&stObjMeta);
    if (s32Ret != CVIAI_SUCCESS) {
      AI_LOGE("inference failed!, ret=%x\n", s32Ret);
      goto inf_error;
    }
    gettimeofday(&t1, NULL);

  inf_error:
    CVI_VPSS_ReleaseChnFrame(0, VPSS_CHN1, &stFrame);
  get_frame_failed:
    if (s32Ret != CVI_SUCCESS) {
      bExit = true;
    }
  }

  AI_LOGI("Exit AI thread\n");
  pthread_exit(NULL);
}

CVI_S32 get_middleware_config(SAMPLE_AI_MW_CONFIG_S *pstMWConfig) {
  // Video Pipeline of this sample:
  //                                                       +------+
  //                                    CHN0 (VBPool 0)    | VENC |--------> RTSP
  //  +----+      +----------------+---------------------> +------+
  //  | VI |----->| VPSS 0 (DEV 1) |            +-----------------------+
  //  +----+      +----------------+----------> | VPSS 1 (DEV 0) AI SDK |------------> AI model
  //                            CHN1 (VBPool 1) +-----------------------+  CHN0 (VBPool 2)

  // VI configuration
  //////////////////////////////////////////////////
  // Get VI configurations from ini file.
  CVI_S32 s32Ret = SAMPLE_AI_Get_VI_Config(&pstMWConfig->stViConfig);
  if (s32Ret != CVI_SUCCESS || pstMWConfig->stViConfig.s32WorkingViNum <= 0) {
    AI_LOGE("Failed to get senor infomation from ini file (/mnt/data/sensor_cfg.ini).\n");
    return -1;
  }

  // Get VI size
  PIC_SIZE_E enPicSize;
  s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(pstMWConfig->stViConfig.astViInfo[0].stSnsInfo.enSnsType,
                                          &enPicSize);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("Cannot get senor size\n");
    return s32Ret;
  }

  SIZE_S stSensorSize;
  s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSensorSize);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("Cannot get senor size\n");
    return s32Ret;
  }

  // Setup frame size of video encoder to 1080p
  SIZE_S stVencSize = {
      .u32Width = 1920,
      .u32Height = 1080,
  };

  // VBPool configurations
  //////////////////////////////////////////////////
  pstMWConfig->stVBPoolConfig.u32VBPoolCount = 3;

  // VBPool 0 for VI
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[0].enFormat = VI_PIXEL_FORMAT;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[0].u32BlkCount = 3;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[0].u32Height = stSensorSize.u32Height;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[0].u32Width = stSensorSize.u32Width;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[0].bBind = true;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[0].u32VpssChnBinding = VPSS_CHN0;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[0].u32VpssGrpBinding = (VPSS_GRP)0;

  // VBPool 1 for AI frame
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[1].enFormat = VI_PIXEL_FORMAT;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[1].u32BlkCount = 3;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[1].u32Height = stVencSize.u32Height;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[1].u32Width = stVencSize.u32Width;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[1].bBind = true;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[1].u32VpssChnBinding = VPSS_CHN1;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[1].u32VpssGrpBinding = (VPSS_GRP)0;

  // VBPool 2 for AI preprocessing.
  // The input pixel format of AI SDK models is eighter RGB 888 or RGB 888 Planar.
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[2].enFormat = PIXEL_FORMAT_RGB_888_PLANAR;
  // AI SDK use only 1 buffer at the same time.
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[2].u32BlkCount = 1;
  // Considering the maximum input size of object detection model is 1024x768, we set same size
  // here.
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[2].u32Height = 768;
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[2].u32Width = 1024;
  // Don't bind with VPSS here, AI SDK would bind this pool automatically when user assign this pool
  // through CVI_AI_SetVBPool.
  pstMWConfig->stVBPoolConfig.astVBPoolSetup[2].bBind = false;

  // VPSS configurations
  //////////////////////////////////////////////////

  // Create a VPSS Grp0 for main stream, video encoder, and AI frame.
  pstMWConfig->stVPSSPoolConfig.u32VpssGrpCount = 1;
  pstMWConfig->stVPSSPoolConfig.stVpssMode.aenInput[0] = VPSS_INPUT_MEM;
  pstMWConfig->stVPSSPoolConfig.stVpssMode.enMode = VPSS_MODE_DUAL;
  pstMWConfig->stVPSSPoolConfig.stVpssMode.ViPipe[0] = 0;
  pstMWConfig->stVPSSPoolConfig.stVpssMode.aenInput[1] = VPSS_INPUT_ISP;
  pstMWConfig->stVPSSPoolConfig.stVpssMode.ViPipe[1] = 0;

  SAMPLE_AI_VPSS_CONFIG_S *pstVpssConfig = &pstMWConfig->stVPSSPoolConfig.astVpssConfig[0];
  pstVpssConfig->bBindVI = true;

  // Assign device 1 to VPSS Grp0, because device1 has 3 outputs in dual mode.
  VPSS_GRP_DEFAULT_HELPER2(&pstVpssConfig->stVpssGrpAttr, stSensorSize.u32Width,
                           stSensorSize.u32Height, VI_PIXEL_FORMAT, 1);

  // Enable two channels for VENC and AI frame
  pstVpssConfig->u32ChnCount = 2;

  // Bind VPSS Grp0 Ch0 with VI
  pstVpssConfig->u32ChnBindVI = VPSS_CHN0;
  VPSS_CHN_DEFAULT_HELPER(&pstVpssConfig->astVpssChnAttr[0], stVencSize.u32Width,
                          stVencSize.u32Height, VI_PIXEL_FORMAT, true);
  VPSS_CHN_DEFAULT_HELPER(&pstVpssConfig->astVpssChnAttr[1], stVencSize.u32Width,
                          stVencSize.u32Height, VI_PIXEL_FORMAT, true);

  // VENC
  //////////////////////////////////////////////////
  // Get default VENC configurations
  SAMPLE_AI_Get_Input_Config(&pstMWConfig->stVencConfig.stChnInputCfg);
  pstMWConfig->stVencConfig.u32FrameWidth = stVencSize.u32Width;
  pstMWConfig->stVencConfig.u32FrameHeight = stVencSize.u32Height;

  // RTSP
  //////////////////////////////////////////////////
  // Get default RTSP configurations
  // SAMPLE_AI_Get_RTSP_Config(&pstMWConfig->stRTSPConfig.stRTSPConfig);

  return s32Ret;
}

static void SampleHandleSig(CVI_S32 signo) {
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  AI_LOGI("handle signal, signo: %d\n", signo);
  if (SIGINT == signo || SIGTERM == signo) {
    bExit = true;
  }
}

int main(int argc, char *argv[]) {
  if (argc != 1) {
    printf("\nUsage: %s \n\n", argv[0]);
    return -1;
  }

  signal(SIGINT, SampleHandleSig);
  signal(SIGTERM, SampleHandleSig);

  //  Step 1: Initialize middleware stuff.
  ////////////////////////////////////////////////////

  // Get middleware configurations including VI, VB, VPSS
  SAMPLE_AI_MW_CONFIG_S stMWConfig = {0};
  CVI_S32 s32Ret = get_middleware_config(&stMWConfig);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("get middleware configuration failed! ret=%x\n", s32Ret);
    return -1;
  }

  // Initialize middleware.
  SAMPLE_AI_MW_CONTEXT stMWContext = {0};
  s32Ret = SAMPLE_AI_Init_WM_NO_RTSP(&stMWConfig, &stMWContext);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("init middleware failed! ret=%x\n", s32Ret);
    return -1;
  }

  app_ipcam_NetCtrl_Init();
  app_ipcam_WebSocket_Init();

  // Step 2: Create and setup AI SDK
  ///////////////////////////////////////////////////

  // Create AI handle and assign VPSS Grp1 Device 0 to AI SDK. VPSS Grp1 is created
  // during initialization of AI SDK.
  cviai_handle_t stAiHandle = NULL;
  GOTO_IF_FAILED(CVI_AI_CreateHandle2(&stAiHandle, 1, 0), s32Ret, create_ai_fail);

  // Assign VBPool ID 2 to the first VPSS in AI SDK.
  GOTO_IF_FAILED(CVI_AI_SetVBPool(stAiHandle, 0, 2), s32Ret, create_service_fail);

  cviai_service_handle_t stServiceHandle = NULL;
  GOTO_IF_FAILED(CVI_AI_Service_CreateHandle(&stServiceHandle, stAiHandle), s32Ret,
                 create_service_fail);

  // Step 3: Run models in thread.
  ///////////////////////////////////////////////////

  pthread_t stVencThread, stAiThread;
  SAMPLE_AI_VENC_THREAD_ARG_S venc_args = {
      .pstMWContext = &stMWContext,
      .stServiceHandle = stServiceHandle,
  };

  SAMPLE_AI_AI_THREAD_ARG_S ai_args = {
      .stAiHandle = stAiHandle,
  };

  pthread_create(&stVencThread, NULL, run_venc_thread, &venc_args);
  pthread_create(&stAiThread, NULL, run_ai_thread, &ai_args);

  // Thread for video encoder
  pthread_join(stVencThread, NULL);
  // Thread for AI inference
  pthread_join(stAiThread, NULL);

  app_ipcam_WebSocket_DeInit();
  app_ipcam_NetCtrl_DeInit();

  CVI_AI_Service_DestroyHandle(stServiceHandle);
create_service_fail:
  CVI_AI_DestroyHandle(stAiHandle);
create_ai_fail:
  SAMPLE_AI_Destroy_MW_NO_RTSP(&stMWContext);

  return 0;
}
