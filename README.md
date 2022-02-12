# 简介
这是一个基于 FFmpeg 实现的极简的直播系统，旨在学习相关知识。

本系统包括 player, recorder, server 三部分，分别用于：
* player: 用于播放音视频数据的客户端。
* recorder: 用于实时录制并上传音视频数据的主播端。
* server：用于连接 player 和 recorder 的服务端。

# player

### 模块划分
从功能上可划分为三个模块：
* reader：从本地文件或网络读取媒体数据。
* decoder：基于 FFmpeg 将媒体数据转为 PCM 和 YUV，并从中获取播放参数。
* writer：基于 SDL 播放 decoder 输出的数据。

### 模块之间的交互方式
在实际使用场景中，reader，decoder 和 writer 模块的速率应当保持一致。

因为当前两者速率低于 writer 的速率时，会造成卡顿。

前两个速率高于 writer 时，会造成内存的浪费，比如 1G 的 AVC 视频封装文件，全部解码后可能 10G 甚至几十G。

因此，需要保证三者的速率保持一致。

reader 和 decoder 可以借助 AVIOContext 实现交互，在此实现中，可在 decoder 的回调函数中执行 reader 操作。因此两者的速率可以保持一致。

decoder 和 writer 的速率可使用生产者-消费者模型保持一致。

decoder 作为生产者，向队列中写入帧数据，变更事件(如帧率，宽高，采样率发生变化时，可通过该机制及时通知 writer)。

writer 作为消费者，从队列中读取数据和参数，并播放。

当队列中数据量高于阈值时，decoder 停止生产；当低于阈值时，decoder 开始生产。

基于上述两个机制，基本可实现 reader，decoder，writer 的速率保持一致。

### 开发计划
分两步走：
* 第一步：基于 FFmpeg 和 SDL 实现播放本地的媒体文件。
* 第二步：实现和 server 通信，播放从 server 获取的媒体数据。

# server

TODO

# recorder

TODO
