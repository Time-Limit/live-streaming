.PHONY: player-client

player-client:
	g++ -o player-client \
		player/main.cc \
		player/reader.cc player/decoder.cc player/speaker.cc player/renderer.cc \
		player/context.cc player/args.cc \
		util/util.cc \
		-std=c++14 \
		-lgflags -lavutil -lavcodec -lavdevice -lavformat -lavfilter -lswscale \
		-lz -lswresample -llzma -liconv -lspeex -lmp3lame -lbz2 -lSDL2 \
		-framework AudioToolBox -framework VideoToolbox -framework CoreFoundation -framework CoreMedia -framework CoreVideo -framework CoreServices -framework Security\
		-I./


