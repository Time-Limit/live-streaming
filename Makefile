.PHONY: player.bin recorder.bin server.bin

CC = g++

args = -Wall -O0 -g -std=c++14
inls = -I./util/net/ -I./util/ -I./
lds = -lgflags -lavutil -lavcodec -lavdevice -lavformat -lavfilter -lswscale -lz -lswresample -llzma -liconv -lspeex -lmp3lame -lbz2 -lSDL2  -lx264 -lpostproc -levent
frameworks = -framework AudioToolBox -framework VideoToolbox -framework CoreFoundation -framework CoreMedia -framework CoreVideo -framework CoreServices	\
						 -framework Security -framework AVFoundation -framework CoreImage -framework AppKit -framework CoreAudio -framework OpenGL -framework Foundation

util_objs = ./util/audio_resample_helper.o ./util/decoder.o ./util/env.o ./util/filter.o ./util/muxer.o ./util/reader.o ./util/renderer.o ./util/speaker.o ./util/util.o ./util/video_scale_helper.o

# 先注释了，这个模块是之前在 Linux 上编写的，内部基于 epoll 实现的，没法在 OS X 上用。使用 libevent 代替吧。
#util_net_objs = ./util/net/log.o ./util/net/neter.o ./util/net/octets.o ./util/net/session.o ./util/net/threadpool.o

player_objs = player/main.o player/context.o player/args.o

player.bin: $(util_objs) $(player_objs)
	g++ -o player.bin $(util_objs) $(player_objs) $(inls) $(args) $(lds) $(frameworks)

recorder_objs = recorder/main.o recorder/context.o recorder/args.o recorder/base.o

recorder.bin: $(util_objs) $(recorder_objs)
	g++ -o recorder.bin $(util_objs) $(recorder_objs) $(inls) $(args) $(lds) $(frameworks)

server_objs = server/main.o server/net.o server/rtmp.o server/stream.o server/command_message.o server/chunk_message.o

server.bin: $(util_objs) $(util_net_objs) $(server_objs)
	g++ -o server.bin $(util_objs) $(util_net_objs) $(server_objs) $(inls) $(args) $(lds) $(frameworks) -g

.cpp.o:
	g++ -c $^ -o $@ $(inls) $(args)

.cc.o:
	g++ -c $^ -o $@ $(inls) $(args)

clean:
	rm -rf $(util_objs) $(util_net_objs) $(server_objs)
