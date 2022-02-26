bavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <SDL2/SDL.h>

#include <vector>

namespace live {
namespace player {

#pragma pack (1)
// 音频相关参数
struct AudioParam {
  int8_t channels = 0; // The number of audio channels
  int32_t sample_rate = 0; // The number of audio samples per second.
  enum AVSampleFormat sample_format;
  bool IsSame(const AudioParam &rhs) const {
    return memcmp(this, &rhs, sizeof(AudioParam)) == 0;
  }
};

// 视频相关参数
struct VideoParam {
  uint32_t height = 0;
  uint32_t width = 0;
  double frame_rate = 0;
  int linewidth = 0;
  enum AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;
  bool IsSame(const VideoParam &rhs) const {
    return memcmp(this, &rhs, sizeof(VideoParam)) == 0;
  }
};
#pragma pack ()

struct Message {
  enum Type {
    UNDEFINED = -1,
    DATA = 0,
    VIDEO_PARAM = 1,
    AUDIO_PARAM = 2
  } type;
  std::vector<uint8_t> data;
};

/*
 * Context 管理了reader, decoder, writer 相关数据
 * 在使用前应调用 InitLocalFileReader，InitDecoder，InitWriter，并确保执行成功。
 */
class Context {
  // --------- reader begin --------- 
  // reader：从本地文件或网络获取媒体数据，通过 AVIOContext 与 decode 交互
  std::unique_ptr<Reader> reader_;

 public:
 /*
  * @param path 本地文件的路径
  * @return 成功 true，失败 false
  */
  bool InitLocalFileReader(const std::string &path);
  // --------- reader end --------- 

  // --------- decoder begin --------- 
 private: 
  // decoder：从 AVIOContext 中获取数据，并将其解码为 PCM 和 YUV，并从中获取播放参数
  // 这一部分主要依赖 FFmpeg，不存在多种实现，因为未做进一步的封装

  // ---- FFmpeg 相关变量开始 ----
  // 以下变量的用法可见 FFmpeg/doc/examples/demuxing_decoding.c

  // 如果想自定义 IO，需在 avformat_open_input 之前设置 AVIOContext，可参见 FFmpeg/doc/examples/avio_reading.c
  AVFormatContext *format_context_ = nullptr;
  
  int video_stream_idx_ = -1;
  int audio_stream_idx_ = -1;

  AVCodecContext *video_dec_ctx_ = nullptr;
  AVCodecContext *audio_dec_ctx_ = nullptr;

  AVFrame *frame_ = nullptr;
  AVPacket *packet_ = nullptr;

  int video_dst_linesize_[4] = {0};

  uint8_t *avio_ctx_buffer_ = nullptr;
  size_t avio_ctx_buffer_size_ = -1;

  AVIOContext *avio_ctx_ = nullptr;

  // ---- FFmpeg 相关变量结束 ----

  void Decode();

 public:
 /*
  * @param format 输入数据的封装格式，一般为文件的后缀名，如 flv，mp4 等
  * @return 成功 true，失败 false
  */
  bool InitDecoder(const std::string &format);

 private:
 /*
  * @param stream_index 待初始化的流的索引
  * @param dec_ctx 待初始化的AVCodecContext
  * @param type AVMEDIA_TYPE_VIDEO 初始化视频流相关参数，AVMEDIA_TYPE_AUDIO 初始化音频流相关参数
  * @return 成功 true，失败 false
  */
  bool InitDecodeContext(int *stream_index, AVCodecContext **dec_ctx, enum AVMediaType type);

  bool DecodePacket(enum AVMediaType type,const AVPacket *pkt, std::vector<Message> *msgs);
  bool GetAudioParamAndDataMessages(AVFrame *frame, std::vector<Message> *msgs);
  bool GetVideoParamAndDataMessages(AVFrame *frame, std::vector<Message> *msgs);

  // --------- decoder end --------- 

  
  // --------- writer begin --------- 
  // writer：使用 SDL 播放解码数据

  // 通过 SDL_OpenAudioDevice 获取的 device_id, 需通过 SDL_CloseAudioDevice 释放
  SDL_AudioDeviceID audio_device_id_ = -1;
  AudioParam current_audio_param_;
  SDL_AudioSpec desired_audio_spec_;
  SDL_AudioSpec obtained_audio_spec_;
  std::vector<uint8_t> pcm_data_buffer_;
  util::Queue<std::vector<uint8_t>> pcm_data_queue_;
  void PlayPCM();
  bool OpenAudioDevice(const AudioParam &param);
  bool CloseAudioDevice();

  static void SDLAudioDeviceCallback(void *userdata, Uint8 *stream, int len) {
    reinterpret_cast<Context *>(userdata)->SDLAudioDeviceCallbackInternal(stream, len);
  }
  void SDLAudioDeviceCallbackInternal(Uint8 *stream, int len);

  VideoParam current_video_param_;
  struct WindowSize {int w; int h;};
  std::atomic<WindowSize> window_size_;
  SDL_Window *window_ = nullptr;
  SDL_Renderer* render_ = nullptr;
  SDL_Texture* texture_ = nullptr;
  bool CreateWindow();
  bool CreateTexture(const VideoParam &param);
  bool DestroyTexture();
  bool DestroyWindow();
  void PlayRawVideo();
  // --------- writer end --------- 
  
  // --------- other --------- 
 private:
 /*
  * @param opaque 自定义数据，此处必传一个 Context 的指针
  * @param buf 缓冲区
  * @param buf_size 缓冲区大小
  * @return 小于0表示错误，其他表示读取的字节数
  *
  * @note 亦可参见 FFmpeg/doc/examples/avio_reading.c
  */
  static int ReadForAVIOContext(void *opaque, uint8_t *buf, int buf_size);
  static int64_t SeekForAVIOContext(void *opaque, int64_t offset, int whence);
 public:
  Context() : pcm_data_queue_(1) {}
  ~Context();
  void Work();

 private:
  // decoder 和 writer 交互的队列
  util::Queue<Message> video_queue_;
  util::Queue<Message> audio_queue_;

  bool read_finish_ = false;
  bool quit_flag_ = false;
  bool has_fatal_error_ = false;

  void FatalErrorOccurred() {
    has_fatal_error_ = true;
    quit_flag_ = true;
  }
};

}
}
