#include "cviai_types_free.h"

void CVI_AI_FreeCpp(cvai_feature_t *feature) {
  if (feature->ptr != NULL) {
    free(feature->ptr);
    feature->ptr = NULL;
  }
  feature->size = 0;
}

void CVI_AI_FreeCpp(cvai_pts_t *pts) {
  if (pts->x != NULL) {
    free(pts->x);
    pts->x = NULL;
  }
  if (pts->y != NULL) {
    free(pts->y);
    pts->y = NULL;
  }
  pts->size = 0;
}

void CVI_AI_FreeCpp(cvai_face_info_t *face_info) {
  CVI_AI_FreeCpp(&face_info->face_pts);
  CVI_AI_FreeCpp(&face_info->face_feature);
}

void CVI_AI_FreeCpp(cvai_face_t *face) { CVI_AI_FreeCpp(face->face_info); }

void CVI_AI_FreeCpp(cvai_object_info_t *face_info) {
  // Do nothing
}

void CVI_AI_FreeCpp(cvai_object_t *face) {
  // Do nothing
}

void CVI_AI_FreeFeature(cvai_feature_t *feature) { CVI_AI_FreeCpp(feature); }

void CVI_AI_FreePts(cvai_pts_t *pts) { CVI_AI_FreeCpp(pts); }

void CVI_AI_FreeFaceInfo(cvai_face_info_t *face_info) {
  CVI_AI_FreeCpp(&face_info->face_pts);
  CVI_AI_FreeCpp(&face_info->face_feature);
}

void CVI_AI_FreeFace(cvai_face_t *face) { CVI_AI_FreeCpp(face->face_info); }

void CVI_AI_FreeObjectInfo(cvai_object_info_t *obj_info) { CVI_AI_FreeCpp(obj_info); }

void CVI_AI_FreeObject(cvai_object_t *obj) { CVI_AI_FreeCpp(obj); }
