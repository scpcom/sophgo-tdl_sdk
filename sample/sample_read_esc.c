#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include "cvi_audio.h"
#include "cviai.h"

#define PERIOD_SIZE 640
#define SAMPLE_RATE 16000
#define FRAME_SIZE SAMPLE_RATE * 2 * 3  // PCM_FORMAT_S16_LE (2bytes) 3 seconds

// ESC class name
char ES_Classes[6][32] = {"Sneezing/Coughing", "Sneezong/Coughing", "Clapping",
                          "Baby Cry",          "Glass breaking",    "Office"};

bool gRun = true;     // signal
bool record = false;  // record to output
char *outpath;        // output file path

// Init cviai handle.
cviai_handle_t ai_handle = NULL;

static void SampleHandleSig(CVI_S32 signo) {
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);

  if (SIGINT == signo || SIGTERM == signo) {
    gRun = false;
  }
}

// Get frame and set it to global buffer
void *thread_uplink_audio(void *arg) {
  CVI_S32 s32Ret;
  AUDIO_FRAME_S stFrame;
  AEC_FRAME_S stAecFrm;
  int loop = SAMPLE_RATE / PERIOD_SIZE * 3;  // 3 seconds
  int size = PERIOD_SIZE * 2;                // PCM_FORMAT_S16_LE (2bytes)

  // Set video frame interface
  CVI_U8 buffer[FRAME_SIZE];  // 3 seconds
  VIDEO_FRAME_INFO_S Frame;
  Frame.stVFrame.pu8VirAddr[0] = buffer;  // Global buffer
  Frame.stVFrame.u32Height = 1;
  Frame.stVFrame.u32Width = FRAME_SIZE;

  // classify the sound result
  int index = -1;

  while (gRun) {
    for (int i = 0; i < loop; ++i) {
      s32Ret = CVI_AI_GetFrame(0, 0, &stFrame, &stAecFrm, CVI_FALSE);  // Get audio frame
      if (s32Ret != CVI_SUCCESS) {
        printf("CVI_AI_GetFrame --> none!!\n");
        continue;
      } else {
        memcpy(buffer + i * size, (CVI_U8 *)stFrame.u64VirAddr[0],
               size);  // Set the period size date to global buffer
      }
      s32Ret = CVI_AI_ReleaseFrame(0, 0, &stFrame, &stAecFrm);
    }
    if (!record) {
      CVI_AI_SoundClassification(ai_handle, &Frame, &index);  // Detect the audio
      // Print esc result
      if (index == 0 || index == 1)
        printf("esc class: %s  \n", ES_Classes[0]);
      else
        printf("esc class: %s  \n", ES_Classes[index]);
    } else {
      FILE *fp = fopen(outpath, "wb");
      fwrite((char *)buffer, 1, FRAME_SIZE, fp);
      fclose(fp);
      gRun = false;
    }
  }
  pthread_exit(NULL);
}

CVI_S32 SET_AUDIO_ATTR(CVI_VOID) {
  // STEP 1: cvitek_audin_set
  //_update_audin_config
  AIO_ATTR_S AudinAttr;
  AudinAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)SAMPLE_RATE;
  AudinAttr.u32ChnCnt = 1;
  AudinAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
  AudinAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
  AudinAttr.enWorkmode = AIO_MODE_I2S_MASTER;
  AudinAttr.u32EXFlag = 0;
  AudinAttr.u32FrmNum = 10;                /* only use in bind mode */
  AudinAttr.u32PtNumPerFrm = PERIOD_SIZE;  // sample rate / fps
  AudinAttr.u32ClkSel = 0;
  AudinAttr.enI2sType = AIO_I2STYPE_INNERCODEC;
  CVI_S32 s32Ret;
  // STEP 2:cvitek_audin_uplink_start
  //_set_audin_config
  s32Ret = CVI_AI_SetPubAttr(0, &AudinAttr);
  if (s32Ret != CVI_SUCCESS) printf("CVI_AI_SetPubAttr failed with %#x!\n", s32Ret);

  s32Ret = CVI_AI_Enable(0);
  if (s32Ret != CVI_SUCCESS) printf("CVI_AI_Enable failed with %#x!\n", s32Ret);

  s32Ret = CVI_AI_EnableChn(0, 0);
  if (s32Ret != CVI_SUCCESS) printf("CVI_AI_EnableChn failed with %#x!\n", s32Ret);

  s32Ret = CVI_AI_SetVolume(0, 4);
  if (s32Ret != CVI_SUCCESS) printf("CVI_AI_SetVolume failed with %#x!\n", s32Ret);

  printf("SET_AUDIO_ATTR success!!\n");
  return CVI_SUCCESS;
}

int main(int argc, char **argv) {
  if (argc != 2 && argc != 4) {
    printf(
        "Usage: %s <esc_model_path> <record 0 or 1> <output file path>\n"
        "\t esc model path\n"
        "\t record, 0: disable 1. enable\n"
        "\t output file path: {output file path}.raw\n",
        argv[0]);
    return CVI_FAILURE;
  }
  // Set signal catch
  signal(SIGINT, SampleHandleSig);
  signal(SIGTERM, SampleHandleSig);

  CVI_S32 ret = CVI_SUCCESS;
  if (CVI_AUDIO_INIT() == CVI_SUCCESS) {
    printf("CVI_AUDIO_INIT success!!\n");
  } else {
    printf("CVI_AUDIO_INIT failure!!\n");
    return 0;
  }

  SET_AUDIO_ATTR();

  ret = CVI_AI_CreateHandle(&ai_handle);

  if (ret != CVI_SUCCESS) {
    printf("Create ai handle failed with %#x!\n", ret);
    return ret;
  }

  ret = CVI_AI_SetModelPath(ai_handle, CVI_AI_SUPPORTED_MODEL_SOUNDCLASSIFICATION, argv[1]);
  if (ret != CVI_SUCCESS) {
    printf("Set model esc failed with %#x!\n", ret);
    return ret;
  }

  if (argc == 4) {
    record = atoi(argv[2]) ? true : false;
    outpath = (char *)malloc(sizeof(argv[3]));
    strcpy(outpath, argv[3]);
  }

  pthread_t pcm_output_thread;
  pthread_create(&pcm_output_thread, NULL, thread_uplink_audio, NULL);

  pthread_join(pcm_output_thread, NULL);
  CVI_AI_DestroyHandle(ai_handle);
  if (argc == 4) {
    free(outpath);
  }

  return 0;
}
