#include "util/reader.h"
#include "util/util.h"
#include "util/base.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>

#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <chrono>
#include <thread>
#include <future>

namespace live {
namespace player {

class Context {
  // --------- reader start --------- 
  
  // reader：从本地文件或网络获取媒体数据，通过 AVIOContext 与 decode 交互
  using Reader = ::live::util::Reader;
  using LocalFileReader = ::live::util::LocalFileReader;
  std::unique_ptr<Reader> reader_;

  // --------- reader end --------- 

  // --------- FFmpeg start ---------
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

 /*
  * @param opaque 自定义数据，此处必传一个 Context 的指针
  * @param buf 缓冲区
  * @param buf_size 缓冲区大小
  * @return 小于0表示错误，其他表示读取的字节数
  *
  * @note 亦可参见 FFmpeg/doc/examples/avio_reading.c
  */
  static int ReadForAVIOContext(void *opaque, uint8_t *buf, int buf_size);

 /*
  * @param opaque 自定义数据，此处必传一个 Context 的指针
  * @param offset, whence 含义可参见 reader.h
  * @return 小于0表示错误。其他时候的函数由 whence 决定，可参见 reader.h
  */
  static int64_t SeekForAVIOContext(void *opaque, int64_t offset, int whence);

 /*
  * @param stream_index 待初始化的流的索引
  * @param dec_ctx 待初始化的AVCodecContext
  * @param type AVMEDIA_TYPE_VIDEO 初始化视频流相关参数，AVMEDIA_TYPE_AUDIO 初始化音频流相关参数
  * @return 成功 true，失败 false
  */
  bool InitDecodeContext(int *stream_index, AVCodecContext **dec_ctx, enum AVMediaType type);

  /*
   * @return 初始化FFmpeg相关变量, true 成功，false 失败
   */
  bool InitFFmpeg();
  // --------- FFmpeg end ---------

  struct TimeInterval {
    int64_t start = -1; // 单位微秒
    int64_t duration = -1; // 单位微秒
    int64_t corresponding_alive_micro_seconds_ = 0; // start 对应的 alive_micro_seconds_ 的值, 用于音画同步
  };
  std::atomic<TimeInterval> playing_time_interval_; // 当前正在播放的音频采样数据的 pts 区间

  int64_t started_playing_micro_second_ = -1; // 开始播放时的 alive_micro_seconds_ 的值
  int64_t alive_micro_seconds_ = 0; // Context 对象创建的时长，单位毫秒。会有一个线程专门更新该字段
  bool is_alive_ = true; // 是否执行析构函数了
  std::future<void> heart_future_; // 用于知悉更新 alive_micro_seconds_ 的线程是否退出

 public:
  AVFormatContext* GetFormatContext() { return format_context_; }
  AVStream* GetAudioStream() { return format_context_->streams[audio_stream_idx_]; }
  AVStream* GetVideoStream() { return format_context_->streams[video_stream_idx_]; }
  AVCodecContext* GetAudioCodecContext() { return audio_dec_ctx_; }
  AVCodecContext* GetVideoCodecContext() { return video_dec_ctx_; }
  bool IsVideoPacket(const AVPacket *p) const { return p && p->stream_index == video_stream_idx_; }
  bool IsAudioPacket(const AVPacket *p) const { return p && p->stream_index == audio_stream_idx_; }

  /*
   * @Param time_point 播放时间点，单位微秒
   * @return 需要延迟的时间，用可能返回负数，表示应加快播放速度。
   *
   * @note 该函数目前只给 Renderer 使用，用于和 Speaker 同步。
   */
  int64_t CalcDelayTimeInMicroSecond(const util::AVFrameWrapper &frame) {
    int64_t time_point = frame->pts * 1000000L * frame->time_base.num / frame->time_base.den;
    TimeInterval ti = playing_time_interval_.load();
    if (time_point <= ti.start) {
      return 0;
    }
    if (ti.start == 0) {
      return 100*1000;
    }
    int64_t delta = 0;
    if (time_point > ti.start + ti.duration + 100*1000) {
      delta += time_point - ti.start;
    }
    //LOG_ERROR << "time_point: " << time_point << ", ti.start: " << ti.start
    //  << ", alive_micro_seconds_: " << alive_micro_seconds_
    //  << ", started_playing_micro_second_: " << started_playing_micro_second_;
    return (time_point - ti.start) - (alive_micro_seconds_ - ti.corresponding_alive_micro_seconds_) + delta;
  }

  /*
   * @Param start, 当前正在播放的音频片段的起始 pts
   * @Param duration, 当前正在播放的音频片段的播放时长
   */
  void UpdatePlayingTimeInterval(const util::AVFrameWrapper &frame) {
    int64_t start = frame->pts * 1000000L * frame->time_base.num / frame->time_base.den;
    int64_t duration = frame->nb_samples / frame->sample_rate;
    playing_time_interval_.store({start, duration, alive_micro_seconds_});
  }

  /*
   * @Param uri, 待播放的媒体文件
   * @Param is_local_file, 表示 uri 是否为本地文件
   *
   * @note 在某些OS上，只能在主线程创建窗口，因此只能在主线程调用Context的构造函数。
   */
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

    heart_future_ = std::async(std::launch::async, [this](){
      auto now = std::chrono::system_clock::now().time_since_epoch();
      auto started_micro_second = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
      while (is_alive_) {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        now = std::chrono::system_clock::now().time_since_epoch();
        auto micro_second = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
        alive_micro_seconds_ = micro_second - started_micro_second;
      }
    });
  }

  /*
   * @note 错误处理函数，暂时没想好如何实现，就先留空吧
   */
  void FatalErrorOccurred() {}

  ~Context() {
    avcodec_free_context(&video_dec_ctx_);
    avcodec_free_context(&audio_dec_ctx_);
    avformat_close_input(&format_context_);

    if (avio_ctx_) {
      av_freep(&avio_ctx_->buffer);
      avio_context_free(&avio_ctx_);
    }

    is_alive_ = false;
    heart_future_.wait();
  }
};

}
}
