#include "draw_rect.hpp"

#include "core_utils.hpp"
#include "opencv2/opencv.hpp"

#include <cvi_sys.h>
#include <string.h>

#define min(x, y) (((x) <= (y)) ? (x) : (y))
#define max(x, y) (((x) >= (y)) ? (x) : (y))

#define DEFAULT_RECT_COLOR_R (53. / 255.)
#define DEFAULT_RECT_COLOR_G (208. / 255.)
#define DEFAULT_RECT_COLOR_B (217. / 255.)
#define DEFAULT_RECT_THINKNESS 4

namespace cviai {
namespace service {

typedef struct {
  float r;
  float g;
  float b;
} color_rgb;

enum { PLANE_Y = 0, PLANE_U, PLANE_V, PLANE_NUM };

static float GetYuvColor(int chanel, color_rgb *color) {
  if (color == NULL) {
    return 0;
  }

  float yuv_color = 0;
  if (chanel == PLANE_Y) {
    yuv_color = (0.257 * color->r) + (0.504 * color->g) + (0.098 * color->b) + 16;
  } else if (chanel == PLANE_U) {
    yuv_color = -(.148 * color->r) - (.291 * color->g) + (.439 * color->b) + 128;
  } else if (chanel == PLANE_V) {
    yuv_color = (0.439 * color->r) - (0.368 * color->g) - (0.071 * color->b) + 128;
  }

  return (yuv_color < 0) ? 0 : ((yuv_color > 255.) ? 255 : yuv_color);
}

// TODO: Need refactor
static void DrawRect(VIDEO_FRAME_INFO_S *frame, float x1, float x2, float y1, float y2,
                     const char *name, color_rgb color, int rect_thinkness) {
  std::string name_str = name;
  int width = frame->stVFrame.u32Width;
  int height = frame->stVFrame.u32Height;
  x1 = max(min(x1, width - 1), 0);
  x2 = max(min(x2, width - 1), 0);
  y1 = max(min(y1, height - 1), 0);
  y2 = max(min(y2, height - 1), 0);

  color.r *= 255;
  color.g *= 255;
  color.b *= 255;
  char color_y = GetYuvColor(PLANE_Y, &color);
  char color_u = GetYuvColor(PLANE_U, &color);
  char color_v = GetYuvColor(PLANE_V, &color);

  CVI_VOID *vir_addr = CVI_NULL;
  size_t image_size =
      frame->stVFrame.u32Length[0] + frame->stVFrame.u32Length[1] + frame->stVFrame.u32Length[2];
  vir_addr = CVI_SYS_MmapCache(frame->stVFrame.u64PhyAddr[0], image_size);
  CVI_U32 plane_offset = 0;

  for (int i = PLANE_Y; i < PLANE_NUM; i++) {
    frame->stVFrame.pu8VirAddr[i] = ((CVI_U8 *)vir_addr) + plane_offset;
    plane_offset += frame->stVFrame.u32Length[i];
    int stride = frame->stVFrame.u32Stride[i];

    int draw_x1 = x1;
    int draw_x2 = x2;
    int draw_y1 = y1;
    int draw_y2 = y2;
    int draw_rect_thinkness = rect_thinkness;
    char draw_color;
    if (i == PLANE_Y) {
      draw_color = color_y;
    } else if (i == PLANE_U) {
      draw_color = color_u;
    } else {
      draw_color = color_v;
    }

    if (i > PLANE_Y) {
      // uv plane has half size
      draw_x1 /= 2;
      draw_x2 /= 2;
      draw_y1 /= 2;
      draw_y2 /= 2;
      draw_rect_thinkness /= 2;
    }

    // draw rect vertical line
    for (int h = draw_y1; h <= draw_y2; ++h) {
      for (int w = draw_x1; w < draw_x1 + draw_rect_thinkness; ++w) {
        memset((void *)(frame->stVFrame.pu8VirAddr[i] + h * stride + w), draw_color,
               sizeof(draw_color));
      }
      for (int w = draw_x2 - draw_rect_thinkness; (w < draw_x2) && (w >= 0); ++w) {
        memset((void *)(frame->stVFrame.pu8VirAddr[i] + h * stride + w), draw_color,
               sizeof(draw_color));
      }
    }

    // draw rect horizontal line
    for (int w = draw_x1; w <= draw_x2; ++w) {
      for (int h = draw_y1; h < draw_y1 + draw_rect_thinkness; ++h) {
        memset((void *)(frame->stVFrame.pu8VirAddr[i] + h * stride + w), draw_color,
               sizeof(draw_color));
      }
      for (int h = draw_y2 - draw_rect_thinkness; (h < draw_y2) && (h >= 0); ++h) {
        memset((void *)(frame->stVFrame.pu8VirAddr[i] + h * stride + w), draw_color,
               sizeof(draw_color));
      }
    }

    cv::Size cv_size = cv::Size(frame->stVFrame.u32Width, frame->stVFrame.u32Height);
    cv::Point cv_point = cv::Point(x1, y1 - 2);
    double font_scale = 2;
    int thickness = 8;
    if (i != 0) {
      cv_size = cv::Size(frame->stVFrame.u32Width / 2, frame->stVFrame.u32Height / 2);
      cv_point = cv::Point(x1 / 2, (y1 - 2) / 2);
      font_scale /= 2;
      // FIXME: Should div but don't know why it's not correct.
      // thickness /= 2;
    }
    // FIXME: Color incorrect.
    cv::Mat image(cv_size, CV_8UC1, frame->stVFrame.pu8VirAddr[i], frame->stVFrame.u32Stride[i]);
    cv::putText(image, name_str, cv_point, cv::FONT_HERSHEY_SIMPLEX, font_scale,
                cv::Scalar(draw_color), thickness, 8);
    frame->stVFrame.pu8VirAddr[i] = NULL;
  }
  CVI_SYS_IonFlushCache(frame->stVFrame.u64PhyAddr[0], vir_addr, image_size);
  CVI_SYS_Munmap(vir_addr, image_size);
}

template <typename T>
int DrawMeta(const T *meta, VIDEO_FRAME_INFO_S *drawFrame) {
  if (meta->size == 0) {
    return CVI_SUCCESS;
  }
  color_rgb rgb_color;
  rgb_color.r = DEFAULT_RECT_COLOR_R;
  rgb_color.g = DEFAULT_RECT_COLOR_G;
  rgb_color.b = DEFAULT_RECT_COLOR_B;
  for (size_t i = 0; i < meta->size; i++) {
    cvai_bbox_t bbox =
        box_rescale(drawFrame->stVFrame.u32Width, drawFrame->stVFrame.u32Height, meta->width,
                    meta->height, meta->info[i].bbox, BOX_RESCALE_TYPE::CENTER);
    DrawRect(drawFrame, bbox.x1, bbox.x2, bbox.y1, bbox.y2, meta->info[i].name, rgb_color,
             DEFAULT_RECT_THINKNESS);
  }
  return CVI_SUCCESS;
}

template int DrawMeta<cvai_face_t>(const cvai_face_t *meta, VIDEO_FRAME_INFO_S *drawFrame);
template int DrawMeta<cvai_object_t>(const cvai_object_t *meta, VIDEO_FRAME_INFO_S *drawFrame);
}  // namespace service
}  // namespace cviai