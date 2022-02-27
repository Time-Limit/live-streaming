#include "player/reader.h"
#include "util/util.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>

#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <SDL2/SDL.h>

#include <vector>

namespace live {
namespace player {

class Context {
  // --------- reader begin --------- 
  
  // reader：从本地文件或网络获取媒体数据，通过 AVIOContext 与 decode 交互
  std::unique_ptr<Reader> reader_;

  // --------- reader end --------- 

  // ---- FFmpeg 相关变量开始 ----
  // 以下变量的用法可见 FFmpeg/doc/examples/demuxing_decoding.c

  // 如果想自定义 IO，需在 avformat_open_input 之前设置 AVIOContext，可参见 FFmpeg/doc/examples/avio_reading.c
  AVFormatContext *format_context_ = nullptr;
  
  int video_stream_idx_ = -1;
  int audio_stream_idx_ = -1;

  AVCodecContext *video_dec_ctx_ = nullptr;
  AVCodecContext *audio_dec_ctx_ = nullptr;

  uint8_t *avio_ctx_buffer_ = nullptr;
  size_t avio_ctx_buffer_size_ = -1;

  AVIOContext *avio_ctx_ = nullptr;

  // ---- FFmpeg 相关变量结束 ----
  
  SDL_Window *window_ = nullptr;

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

 /*
  * @param stream_index 待初始化的流的索引
  * @param dec_ctx 待初始化的AVCodecContext
  * @param type AVMEDIA_TYPE_VIDEO 初始化视频流相关参数，AVMEDIA_TYPE_AUDIO 初始化音频流相关参数
  * @return 成功 true，失败 false
  */
  bool InitDecodeContext(int *stream_index, AVCodecContext **dec_ctx, enum AVMediaType type);

  bool InitFFmpeg();

  bool InitSDL() {
    if (0 == SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER)) {
      return true;
    }
    LOG_ERROR << "SDL_Init failed, " << SDL_GetError();
    return false;
  }

  bool CreateWindow();

 public:
  AVFormatContext* GetFormatContext() { return format_context_; }
  AVStream* GetAudioStream() { return format_context_->streams[audio_stream_idx_]; }
  AVStream* GetVideoStream() { return format_context_->streams[video_stream_idx_]; }
  AVCodecContext* GetAudioCodecContext() { return audio_dec_ctx_; }
  AVCodecContext* GetVideoCodecContext() { return video_dec_ctx_; }
  bool IsVideoPacket(const AVPacket *p) const { return p && p->stream_index == video_stream_idx_; }
  bool IsAudioPacket(const AVPacket *p) const { return p && p->stream_index == audio_stream_idx_; }
  SDL_Window* GetWindow() { return window_; }

  Context(const std::string uri, bool is_local_file) {
    if (is_local_file) {
      try {
        reader_.reset(new LocalFileReader(uri));
      } catch (const std::string &err) {
        LOG_ERROR << err;
        throw std::string("init reader failed");
      }
    }

    if (!InitFFmpeg()) {
      throw std::string("init ffmpeg failed");
    }

    if (!InitSDL()) {
      throw std::string("init SDL failed");
    }

    if (!CreateWindow()) {
    }
  }

  void FatalErrorOccurred() {}

  ~Context() {
    avcodec_free_context(&video_dec_ctx_);
    avcodec_free_context(&audio_dec_ctx_);
    avformat_close_input(&format_context_);

    if (avio_ctx_) {
      av_freep(&avio_ctx_->buffer);
      avio_context_free(&avio_ctx_);
    }

    SDL_DestroyWindow(window_);
    window_ = nullptr;
    SDL_Quit();
  }
};

}
}
