.PHONY: player-client

player-client:
	g++ -o player-client \
		util/util.cc player/args.cc player/player.cc player/reader.cc player/context.cc\
		-std=c++14 \
		-lgflags -lavutil -lavcodec -lavdevice -lavformat -lavfilter -lz -lswresample -llzma -liconv -lspeex -lmp3lame -lbz2\
		-framework AudioToolBox -framework VideoToolbox -framework CoreFoundation -framework CoreMedia -framework CoreVideo -framework CoreServices -framework Security\
		-I./ \
		-g -O0;


