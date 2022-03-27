.PHONY: player-client recorder-client

player-client:
	g++ -o player-client \
		player/main.cc player/context.cc player/args.cc \
		util/reader.cc util/decoder.cc util/speaker.cc util/renderer.cc util/env.cc \
		util/util.cc util/video_scale_helper.cc util/audio_resample_helper.cc \
		-std=c++14 \
		-lgflags -lavutil -lavcodec -lavdevice -lavformat -lavfilter -lswscale \
		-lz -lswresample -llzma -liconv -lspeex -lmp3lame -lbz2 -lSDL2 \
		-framework AudioToolBox -framework VideoToolbox -framework CoreFoundation -framework CoreMedia -framework CoreVideo -framework CoreServices -framework Security -framework AVFoundation \
		-framework CoreImage -framework AppKit -framework CoreAudio -framework OpenGL -framework Foundation \
		-I. -O3

recorder-client:
	g++ -o recorder-client \
		recorder/main.cc \
		recorder/context.cc recorder/args.cc recorder/base.cc \
		util/reader.cc util/decoder.cc util/speaker.cc util/renderer.cc util/env.cc util/filter.cc util/muxer.cc \
		util/video_scale_helper.cc  util/audio_resample_helper.cc \
		-std=c++14 \
		-lgflags -lavutil -lavcodec -lavdevice -lavformat -lavfilter -lswscale \
		-lz -lswresample -llzma -liconv -lspeex -lmp3lame -lbz2 -lSDL2 \
		-framework AudioToolBox -framework VideoToolbox -framework CoreFoundation -framework CoreMedia -framework CoreVideo -framework CoreServices -framework Security -framework AVFoundation \
		-framework CoreImage -framework AppKit -framework CoreAudio -framework OpenGL -framework Foundation\
		-I. -O3
