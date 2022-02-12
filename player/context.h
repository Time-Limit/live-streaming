extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mem.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "util/util.h"
#include "player/reader.h"

namespace live {
namespace player {

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

  uint8_t *video_dst_data_[4] = {nullptr};
  int video_dst_linesize_[4] = {0};

  uint8_t *avio_ctx_buffer_ = nullptr;
  size_t avio_ctx_buffer_size_ = -1;

  AVIOContext *avio_ctx_ = nullptr;

  // ---- FFmpeg 相关变量结束 ----

 public:
 /*
  * @param format 输入数据的封装格式，一般为文件的后缀名，如 flv，mp4 等
  * @return 成功 true，失败 false
  */
  bool InitDecoder(const std::string &format);

  // --------- decoder end --------- 

  
  // --------- writer begin --------- 
  // writer：使用 SDL 播放解码数据
  // --------- writer end --------- 
  
  // --------- other --------- 
  static int ReadPacketForAVIOContext(void *opaque, uint8_t *buf, int buf_size);
  ~Context();

 private:
 /*
  * @param stream_index 待初始化的流的索引
  * @param dec_ctx 待初始化的AVCodecContext
  * @param type AVMEDIA_TYPE_VIDEO 初始化视频流相关参数，AVMEDIA_TYPE_AUDIO 初始化音频流相关参数
  * @return 成功 true，失败 false
  */
  bool InitDecodeContext(int *stream_index, AVCodecContext **dec_ctx, enum AVMediaType type);
};

}
}
