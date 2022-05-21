#ifndef _CVI_CORE_ERROR_H_
#define _CVI_CORE_ERROR_H_

#include "cvi_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CVIAI Error Code Definition */
/***********************************************************/
/*|-------------------------------------------------------|*/
/*| 11|   MODULE_ID   |   FUNC_ID    |   ERR_ID           |*/
/*|-------------------------------------------------------|*/
/*|<--><----8bits----><-----8bits----><------8bits------->|*/
/***********************************************************/
#define CVI_AI_DEF_ERR(module, func, errid) \
  ((CVI_S32)(0xC0000000L | ((module) << 16) | ((func) << 8) | (errid)))

// clang-format off
typedef enum _MODULE_ID {
  CVIAI_MODULE_ID_CORE       = 1,
  CVIAI_MODULE_ID_SERVICE    = 2,
  CVIAI_MODULE_ID_EVALUATION = 3,
} MODULE_ID;

typedef enum _CVIAI_FUNC_ID {
  CVIAI_FUNC_ID_CORE                       = 1,
  CVIAI_FUNC_ID_DEEPSORT                   = 2,
  CVIAI_FUNC_ID_ALPHA_POSE                 = 3,
  CVIAI_FUNC_ID_FACE_RECORGNITION          = 4,
  CVIAI_FUNC_ID_FACE_LANDMARKER            = 5,
  CVIAI_FUNC_ID_FACE_QUALITY               = 6,
  CVIAI_FUNC_ID_FALL_DETECTION             = 7,
  CVIAI_FUNC_ID_INCAR_OBJECT_DETECTION     = 8,
  CVIAI_FUNC_ID_LICENSE_PLATE_DETECTION    = 9,
  CVIAI_FUNC_ID_LICENSE_PLATE_RECOGNITION  = 10,
  CVIAI_FUNC_ID_LIVENESS                   = 11,
  CVIAI_FUNC_ID_MASK_CLASSIFICATION        = 12,
  CVIAI_FUNC_ID_MASK_FACE_RECOGNITION      = 13,
  CVIAI_FUNC_ID_MOTION_DETECTION           = 14,
  CVIAI_FUNC_ID_MOBILEDET                  = 15,
  CVIAI_FUNC_ID_YOLOV3                     = 16,
  CVIAI_FUNC_ID_OSNET                      = 17,
  CVIAI_FUNC_ID_RETINAFACE                 = 18,
  CVIAI_FUNC_ID_SEGMENTATION               = 19,
  CVIAI_FUNC_ID_SMOKE_CLASSIFICATION       = 20,
  CVIAI_FUNC_ID_SOUND_CLASSIFICATION       = 21,
  CVIAI_FUNC_ID_TAMPER_DETECTION           = 22,
  CVIAI_FUNC_ID_THERMAL_FACE_DETECTION     = 23,
  CVIAI_FUNC_ID_YAWN_CLASSIFICATION        = 24,
  CVIAI_FUNC_ID_THERMAL_PERSON_DETECTION   = 25,
} CVIAI_FUNC_ID;

typedef enum _CVIAI_CORE_ERROR_ID {
  EN_CVIAI_INVALID_MODEL_PATH         = 1,
  EN_CVIAI_FAILED_OPEN_MODEL          = 2,
  EN_CVIAI_FAILED_CLOSE_MODEL         = 3,
  EN_CVIAI_FAILED_GET_VPSS_CHN_CONFIG = 4,
  EN_CVIAI_FAILED_INFERENCE           = 5,
  EN_CVIAI_INVALID_ARGS               = 6,
  EN_CVIAI_FAILED_VPSS_INIT           = 7,
  EN_CVIAI_FAILED_VPSS_SEND_FRAME     = 8,
  EN_CVIAI_FAILED_VPSS_GET_FRAME      = 9,
  EN_CVIAI_MODEL_INITIALIZED          = 10,
  EN_CVIAI_NOT_YET_INITIALIZED        = 11,
  EN_CVIAI_NOT_YET_IMPLEMENTED        = 12,
  EN_CVIAI_ERR_ALLOC_ION_FAIL         = 13,
} CVIAI_CORE_ERROR_ID;

typedef enum _CVIAI_MD_ERROR_ID {
  EN_CVIAI_OPER_FAILED = 1,
} CVIAI_MD_ERROR_ID;

/** @enum CVIAI_RC_CODE
 * @ingroup core_cviaicore
 * @brief Return code for CVIAI.
 */
typedef enum _CVIAI_RC_CODE {
  /* Return code for general condition*/
  // Success
  CVIAI_SUCCESS                     = CVI_SUCCESS,
  // General failure
  CVIAI_FAILURE                     = CVI_FAILURE,
  // Invalid DNN model path
  CVIAI_ERR_INVALID_MODEL_PATH      = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_INVALID_MODEL_PATH),
  // Failed to open DNN model
  CVIAI_ERR_OPEN_MODEL              = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_FAILED_OPEN_MODEL),
  // Failed to close DNN model
  CVIAI_ERR_CLOSE_MODEL             = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_FAILED_CLOSE_MODEL),
  // Failed to get vpss channel config for DNN model
  CVIAI_ERR_GET_VPSS_CHN_CONFIG     = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_FAILED_GET_VPSS_CHN_CONFIG),
  // Failed to inference
  CVIAI_ERR_INFERENCE               = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_FAILED_INFERENCE),
  // Invalid model arguments
  CVIAI_ERR_INVALID_ARGS            = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_INVALID_ARGS),
  // Failed to initialize VPSS
  CVIAI_ERR_INIT_VPSS               = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_FAILED_VPSS_INIT),
  // VPSS send frame fail
  CVIAI_ERR_VPSS_SEND_FRAME         = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_FAILED_VPSS_SEND_FRAME),
  // VPSS get frame fail
  CVIAI_ERR_VPSS_GET_FRAME          = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_FAILED_VPSS_GET_FRAME),
  // Model has initialized
  CVIAI_ERR_MODEL_INITIALIZED       = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_MODEL_INITIALIZED),
  // Not yet initialized
  CVIAI_ERR_NOT_YET_INITIALIZED     = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_NOT_YET_INITIALIZED),
  // Not yet implemented
  CVIAI_ERR_NOT_YET_IMPLEMENTED     = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_NOT_YET_IMPLEMENTED),
  // Failed to allocate ION
  CVIAI_ERR_ALLOC_ION_FAIL          = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_CORE, EN_CVIAI_ERR_ALLOC_ION_FAIL),
  /* Algorithm specific return code */

  // Operation failed of Motion Detection
  CVIAI_ERR_MD_OPERATION_FAILED     = CVI_AI_DEF_ERR(CVIAI_MODULE_ID_CORE, CVIAI_FUNC_ID_MOTION_DETECTION, EN_CVIAI_OPER_FAILED),

} CVIAI_RC_CODE;
// clang-format on

#ifdef __cplusplus
}
#endif

#endif
