# 简介
这是一个基于 FFmpeg 实现的极简的直播系统，旨在学习相关知识。

本系统包括 player, recorder, server 三部分，分别用于：
* player: 用于播放音视频数据的客户端。
* recorder: 用于实时录制并上传音视频数据的主播端。
* server：用于连接 player 和 recorder 的服务端。

# player

### 模块划分
从功能上可划分为三个模块：
* read：从本地文件或网络读取媒体数据。
* decode：基于 FFmpeg 将媒体数据转为 PCM 和 YUV，并从中获取播放canshu。
* play：基于 SDL 播放 decode 输出的数据。

### 开发计划
分两步走：
* 第一步：基于 FFmpeg 和 SDL 实现播放本地的媒体文件。
* 第二步：实现和 server 通信，播放从 server 获取的媒体数据。


# server

TODO

# recorder

TODO
