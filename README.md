# 简介
这是一个基于 FFmpeg，SDL，libevent 实现的极简的直播系统，旨在入门音视频开发。**目前代码只能在 macOS 上通过编译**。

本系统包括 player, recorder, server 三部分，分别用于：
* player: 用于播放音视频数据的客户端。
* recorder: 用于实时录制并上传音视频数据的主播端。
* server：使用 RTMP 协议与 player、recorder 进行通信。

# 使用
首先，需编译各模块
```shell
 make recorder.bin server.bin player.bin -j
```
接着，依次启动 server.bin，recorder.bin，player.bin。

## server
```shell
./server.bin -port 9527
```
## recorder
```shell
./recorder -url rtmp://127.0.0.1:9527 #将多媒体数据推送至RTMP服务器
./recorder -url /path/to/file #将多媒体数据写入磁盘文件
```
## player
```shell
./player.bin -uri /path/to/file #播放本地文件
./player.bin -uri rtmp://127.0.0.1:9527?room=3 #播放来自RTMP服务器的多媒体数据，在当前实现中，需要通过 room=3 指定直播间ID。
```

# TODO
* server 的代码组织太过混乱，需要重新梳理。
* server 只能通过 `kill -9` 退出，需要优化。
* server 的计算，IO均在同一线程，应尝试改为多线程。
* 尝试支持 RTSP，HLS 协议。
* recorder 在录制屏幕时 CPU 使用率过高，与使用腾讯会议等软件的差距过大，需要研判下原因，了解下业界的优化方案。




