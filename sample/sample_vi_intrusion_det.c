#define LOG_TAG "SampleInstrusionDet"
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

static volatile bool bExit = false;

static cvai_object_t g_stObjNoIntrusion = {0};
static cvai_object_t g_stObjIntrusion = {0};

MUTEXAUTOLOCK_INIT(ResultMutex);

/**
 * @brief Arguments for video encoder thread
 *
 */
typedef struct {
  SAMPLE_AI_MW_CONTEXT *pstMWContext;
  cviai_service_handle_t stServiceHandle;
} SAMPLE_AI_VENC_THREAD_ARG_S;

/**
 * @brief Arguments for ai thread
 *
 */
typedef struct {
  ODInferenceFunc inference_func;
  CVI_AI_SUPPORTED_MODEL_E enOdModelId;
  cviai_handle_t stAIHandle;
  cviai_service_handle_t stServiceHandle;
} SAMPLE_AI_AI_THREAD_ARG_S;

void *run_venc(void *args) {
  AI_LOGI("Enter encoder thread\n");
  SAMPLE_AI_VENC_THREAD_ARG_S *pstArgs = (SAMPLE_AI_VENC_THREAD_ARG_S *)args;
  VIDEO_FRAME_INFO_S stFrame;
  CVI_S32 s32Ret;
  cvai_object_t stObjIntrusion = {0};
  cvai_object_t stObjNoIntrusion = {0};

  cvai_service_brush_t stRedBrush = CVI_AI_Service_GetDefaultBrush();
  stRedBrush.color.r = 255;
  stRedBrush.color.g = 0;
  stRedBrush.color.b = 0;

  cvai_service_brush_t stRegionBrush = CVI_AI_Service_GetDefaultBrush();
  stRegionBrush.color.r = 0;
  stRegionBrush.color.g = 255;
  stRegionBrush.color.b = 255;

  // Get the vertices of convex we stored by using CVI_AI_Service_Polygon_SetTarget.
  cvai_pts_t **pastConvexPts = NULL;
  uint32_t u32ConvexNum;
  s32Ret =
      CVI_AI_Service_Polygon_GetTarget(pstArgs->stServiceHandle, &pastConvexPts, &u32ConvexNum);
  if (s32Ret != CVIAI_SUCCESS) {
    AI_LOGE("Cannot get polygon target\n");
    pthread_exit(NULL);
  }

  while (bExit == false) {
    s32Ret = CVI_VPSS_GetChnFrame(0, VPSS_CHN0, &stFrame, 2000);
    if (s32Ret != CVI_SUCCESS) {
      AI_LOGE("CVI_VPSS_GetChnFrame chn0 failed with %#x\n", s32Ret);
      break;
    }

    {
      // Get detection result from global
      MutexAutoLock(ResultMutex, lock);
      CVI_AI_CopyObjectMeta(&g_stObjIntrusion, &stObjIntrusion);
      CVI_AI_CopyObjectMeta(&g_stObjNoIntrusion, &stObjNoIntrusion);
    }

    // Draw pre-defined regions
    for (uint32_t i = 0; i < u32ConvexNum; i++) {
      CVI_AI_Service_DrawPolygon(pstArgs->stServiceHandle, &stFrame, pastConvexPts[i],
                                 stRegionBrush);
    }

    // Draw intrusion and non-intrusion result
    GOTO_IF_FAILED(CVI_AI_Service_ObjectDrawRect(pstArgs->stServiceHandle, &stObjNoIntrusion,
                                                 &stFrame, false, CVI_AI_Service_GetDefaultBrush()),
                   s32Ret, error);

    GOTO_IF_FAILED(CVI_AI_Service_ObjectDrawRect(pstArgs->stServiceHandle, &stObjIntrusion,
                                                 &stFrame, false, stRedBrush),
                   s32Ret, error);

    // Encode frame and send to RTSP
    SAMPLE_AI_Send_Frame_RTSP(&stFrame, pstArgs->pstMWContext);
  error:
    CVI_AI_Free(&stObjIntrusion);
    CVI_AI_Free(&stObjNoIntrusion);
    CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
    if (s32Ret != CVI_SUCCESS) {
      bExit = true;
    }
  }

  free(pastConvexPts);
  AI_LOGI("Exit encoder thread\n");
  pthread_exit(NULL);
}

void seperate_bbox(cvai_object_t *pstObjMeta, cvai_object_t *pstObjNoIntrusion,
                   cvai_object_t *pstObjIntrusion, bool *aIntrusion, uint32_t u32IntrusionCount) {
  pstObjIntrusion->height = pstObjMeta->height;
  pstObjIntrusion->rescale_type = pstObjMeta->rescale_type;
  pstObjIntrusion->width = pstObjMeta->width;
  pstObjIntrusion->size = u32IntrusionCount;
  if (u32IntrusionCount > 0) {
    pstObjIntrusion->info =
        (cvai_object_info_t *)malloc(sizeof(cvai_object_info_t) * u32IntrusionCount);
    memset(pstObjIntrusion->info, 0, sizeof(cvai_object_info_t) * u32IntrusionCount);
  } else {
    pstObjIntrusion->info = NULL;
  }

  pstObjNoIntrusion->height = pstObjMeta->height;
  pstObjNoIntrusion->rescale_type = pstObjMeta->rescale_type;
  pstObjNoIntrusion->width = pstObjMeta->width;
  pstObjNoIntrusion->size = pstObjMeta->size - u32IntrusionCount;
  if (pstObjNoIntrusion->size > 0) {
    pstObjNoIntrusion->info =
        (cvai_object_info_t *)malloc(sizeof(cvai_object_info_t) * pstObjNoIntrusion->size);
    memset(pstObjNoIntrusion->info, 0, sizeof(cvai_object_info_t) * pstObjNoIntrusion->size);
  } else {
    pstObjNoIntrusion->info = NULL;
  }

  uint32_t u32IndexIntrusion = 0;
  uint32_t u32IndexNoIntrusion = 0;
  for (uint32_t i = 0; i < pstObjMeta->size; i++) {
    cvai_object_info_t *pstInfo;
    if (aIntrusion[i]) {
      pstInfo = &pstObjIntrusion->info[u32IndexIntrusion];
      u32IndexIntrusion++;
    } else {
      pstInfo = &pstObjNoIntrusion->info[u32IndexNoIntrusion];
      u32IndexNoIntrusion++;
    }
    CVI_AI_CopyInfo(&pstObjMeta->info[i], pstInfo);
  }
}

void setup_regions(cviai_service_handle_t *stServiceHandle) {
  // This sample setup three regions. Any type of polygon is acceptable including convex and
  // non-convex.
  cvai_pts_t test_region_0;
  cvai_pts_t test_region_1;
  cvai_pts_t test_region_2;

  float r0[2][8] = {{0, 50, 0, 100, 200, 150, 200, 100}, {0, 100, 200, 150, 200, 100, 0, 50}};
  float r1[2][5] = {{380, 560, 500, 320, 260}, {160, 250, 500, 580, 220}};
  float r2[2][4] = {{780, 880, 840, 675}, {400, 420, 620, 580}};

  // Region 0
  test_region_0.size = (uint32_t)sizeof(r0) / (sizeof(float) * 2);
  test_region_0.x = malloc(sizeof(float) * test_region_0.size);
  test_region_0.y = malloc(sizeof(float) * test_region_0.size);
  memcpy(test_region_0.x, r0[0], sizeof(float) * test_region_0.size);
  memcpy(test_region_0.y, r0[1], sizeof(float) * test_region_0.size);

  // Region 1
  test_region_1.size = (uint32_t)sizeof(r1) / (sizeof(float) * 2);
  test_region_1.x = malloc(sizeof(float) * test_region_1.size);
  test_region_1.y = malloc(sizeof(float) * test_region_1.size);
  memcpy(test_region_1.x, r1[0], sizeof(float) * test_region_1.size);
  memcpy(test_region_1.y, r1[1], sizeof(float) * test_region_1.size);

  // Region 2
  test_region_2.size = (uint32_t)sizeof(r2) / (sizeof(float) * 2);
  test_region_2.x = malloc(sizeof(float) * test_region_2.size);
  test_region_2.y = malloc(sizeof(float) * test_region_2.size);
  memcpy(test_region_2.x, r2[0], sizeof(float) * test_region_2.size);
  memcpy(test_region_2.y, r2[1], sizeof(float) * test_region_2.size);

  // Set regions to AI SDK.
  CVI_AI_Service_Polygon_SetTarget(stServiceHandle, &test_region_0);
  CVI_AI_Service_Polygon_SetTarget(stServiceHandle, &test_region_1);
  CVI_AI_Service_Polygon_SetTarget(stServiceHandle, &test_region_2);

  CVI_AI_Free(&test_region_0);
  CVI_AI_Free(&test_region_1);
  CVI_AI_Free(&test_region_2);
}

void *run_ai_thread(void *args) {
  AI_LOGI("Enter AI thread\n");
  SAMPLE_AI_AI_THREAD_ARG_S *pstAIArgs = (SAMPLE_AI_AI_THREAD_ARG_S *)args;
  VIDEO_FRAME_INFO_S stFrame;
  cvai_object_t stObjNoIntrusion = {0};
  cvai_object_t stObjIntrusion = {0};
  cvai_object_t stObjMeta = {0};

  CVI_S32 s32Ret;
  while (bExit == false) {
    s32Ret = CVI_VPSS_GetChnFrame(0, VPSS_CHN1, &stFrame, 2000);

    if (s32Ret != CVI_SUCCESS) {
      AI_LOGE("CVI_VPSS_GetChnFrame failed with %#x\n", s32Ret);
      goto get_frame_failed;
    }

    // Detect objects first.
    GOTO_IF_FAILED(pstAIArgs->inference_func(pstAIArgs->stAIHandle, &stFrame, &stObjMeta), s32Ret,
                   inf_error);

    bool *aIntrusion = NULL;
    if (stObjMeta.size > 0) {
      aIntrusion = (bool *)malloc(stObjMeta.size * sizeof(bool));
      uint32_t u32IntrusionCount = 0;

      // Check which bbox has intersection with pre-defined regions.
      for (uint32_t i = 0; i < stObjMeta.size; i++) {
        bool bIntrusion;
        cvai_bbox_t stBbox = stObjMeta.info[i].bbox;
        GOTO_IF_FAILED(
            CVI_AI_Service_Polygon_Intersect(pstAIArgs->stServiceHandle, &stBbox, &bIntrusion),
            s32Ret, inter_error);
        aIntrusion[i] = bIntrusion;
        if (bIntrusion) {
          u32IntrusionCount++;
        }
      }

      // Seperate detections into intrusion and non-intrusion set.
      seperate_bbox(&stObjMeta, &stObjNoIntrusion, &stObjIntrusion, aIntrusion, u32IntrusionCount);
    }

    AI_LOGI("object count: %d\n", stObjMeta.size);
    {
      // Copy object detection results to global.
      MutexAutoLock(ResultMutex, lock);
      CVI_AI_CopyObjectMeta(&stObjNoIntrusion, &g_stObjNoIntrusion);
      CVI_AI_CopyObjectMeta(&stObjIntrusion, &g_stObjIntrusion);
    }

  inter_error:
    free(aIntrusion);
  inf_error:
    CVI_VPSS_ReleaseChnFrame(0, 1, &stFrame);
  get_frame_failed:
    CVI_AI_Free(&stObjMeta);
    CVI_AI_Free(&stObjIntrusion);
    CVI_AI_Free(&stObjNoIntrusion);
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
  SAMPLE_AI_Get_RTSP_Config(&pstMWConfig->stRTSPConfig.stRTSPConfig);

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
  if (argc != 4 && argc != 3) {
    printf(
        "\nUsage: %s MODEL_NAME MODEL_PATH [THRESHOLD].\n\n"
        "\tMODEL_NAME, detection model name should be one of {mobiledetv2-person-vehicle, "
        "mobiledetv2-person-pets, "
        "mobiledetv2-coco80, "
        "mobiledetv2-vehicle, "
        "mobiledetv2-pedestrian, "
        "yolov3}.\n"
        "\tMODEL_PATH, cvimodel path.\n"
        "\tTHRESHOLD (optional), threshold for detection model (default: 0.5).\n",
        argv[0]);
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
  s32Ret = SAMPLE_AI_Init_WM(&stMWConfig, &stMWContext);
  if (s32Ret != CVI_SUCCESS) {
    AI_LOGE("init middleware failed! ret=%x\n", s32Ret);
    return -1;
  }

  // Step 2: Create and setup AI SDK
  ///////////////////////////////////////////////////

  // Create AI handle and assign VPSS Grp1 Device 0 to AI SDK. VPSS Grp1 is created
  // during initialization of AI SDK.
  cviai_handle_t stAIHandle = NULL;
  GOTO_IF_FAILED(CVI_AI_CreateHandle2(&stAIHandle, 1, 0), s32Ret, create_ai_fail);

  // Assign VBPool ID 2 to the first VPSS in AI SDK.
  GOTO_IF_FAILED(CVI_AI_SetVBPool(stAIHandle, 0, 2), s32Ret, create_service_fail);

  CVI_AI_SetVpssTimeout(stAIHandle, 1000);

  cviai_service_handle_t stServiceHandle = NULL;
  GOTO_IF_FAILED(CVI_AI_Service_CreateHandle(&stServiceHandle, stAIHandle), s32Ret,
                 create_service_fail);

  // Step 3: Open and setup AI models
  ///////////////////////////////////////////////////

  // Get inference function pointer and model id of object deteciton according to model name.
  ODInferenceFunc inference_func;
  CVI_AI_SUPPORTED_MODEL_E enOdModelId;
  if (get_od_model_info(argv[1], &enOdModelId, &inference_func) == CVIAI_FAILURE) {
    AI_LOGE("unsupported model: %s\n", argv[1]);
    return -1;
  }

  GOTO_IF_FAILED(CVI_AI_OpenModel(stAIHandle, enOdModelId, argv[2]), s32Ret, setup_ai_fail);

  if (argc == 4) {
    float fThreshold = atof(argv[3]);
    if (fThreshold < 0.0 || fThreshold > 1.0) {
      AI_LOGE("wrong threshold value: %f\n", fThreshold);
      s32Ret = CVI_FAILURE;
      goto setup_ai_fail;
    } else {
      AI_LOGE("set threshold to %f\n", fThreshold);
    }
    GOTO_IF_FAILED(CVI_AI_SetModelThreshold(stAIHandle, enOdModelId, fThreshold), s32Ret,
                   setup_ai_fail);
  }

  // Select which classes we want to focus.
  GOTO_IF_FAILED(CVI_AI_SelectDetectClass(stAIHandle, enOdModelId, 2, CVI_AI_DET_TYPE_PERSON,
                                          CVI_AI_DET_GROUP_VEHICLE),
                 s32Ret, setup_ai_fail);

  // Setup alarm regions
  setup_regions(stServiceHandle);

  // Step 4: Run models in thread.
  ///////////////////////////////////////////////////

  pthread_t stVencThread, stAIThread;
  SAMPLE_AI_VENC_THREAD_ARG_S venc_args = {
      .pstMWContext = &stMWContext,
      .stServiceHandle = stServiceHandle,
  };

  SAMPLE_AI_AI_THREAD_ARG_S ai_args = {
      .enOdModelId = enOdModelId,
      .inference_func = inference_func,
      .stAIHandle = stAIHandle,
      .stServiceHandle = stServiceHandle,
  };

  pthread_create(&stVencThread, NULL, run_venc, &venc_args);
  pthread_create(&stAIThread, NULL, run_ai_thread, &ai_args);

  // Thread for video encoder
  pthread_join(stVencThread, NULL);

  // Thread for AI inference
  pthread_join(stAIThread, NULL);

setup_ai_fail:
  CVI_AI_Service_DestroyHandle(stServiceHandle);
create_service_fail:
  CVI_AI_DestroyHandle(stAIHandle);
create_ai_fail:
  SAMPLE_AI_Destroy_MW(&stMWContext);

  return 0;
}