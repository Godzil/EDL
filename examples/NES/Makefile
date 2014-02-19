all: nes

GLINCS=`pkg-config --cflags glfw3`
#-I../../../glfw-3.0.4/include
GLLIBS=`pkg-config --libs glfw3` -lGL -lGLU -lm
#-L../../../glfw-3.0.4/lib-mingw -lglfw3 -lglu32 -lopengl32 -lgdi32
ALINCS=`pkg-config openal --cflags`
#-I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include
ALLIBS=`pkg-config openal --libs`
#/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
ZINCS=-I.
#-I../../../zlib125
ZLIB= -lz
EDL=../../bin/edl

PROF_FLAGS=

TV_DECODER_DEPS=jake/ntscDecode.c jake/ntscDecode.h jake/ntscDecodeCrtsim.c jake/ntscDecodeCrtsim.h jake/ntscMatrix.c jake/ntscMatrix.h jake/ntscSvd.c
TV_DECODER=jake/ntscDecode.c jake/ntscDecodeCrtsim.c jake/ntscMatrix.c jake/ntscSvd.c

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) nes.exe

out/rp2c02.lls:	../../chips/Accurate/rp2c02.edl
	mkdir -p out
	$(EDL) -O2 -s PPU_ ../../chips/Accurate/rp2c02.edl >out/rp2c02.lls

out/rp2c02.lls.s: out/rp2c02.lls
	llc -O3 out/rp2c02.lls

out/rp2a03.lls: ../../chips/Accurate/rp2a03.edl
	mkdir -p out
	$(EDL) -O2 -s MAIN_ ../../chips/Accurate/rp2a03.edl >out/rp2a03.lls

out/rp2a03.lls.s: out/rp2a03.lls
	llc -O3 out/rp2a03.lls

out/rp2a03.o: out/rp2a03.lls.s
	clang -O3 -c out/rp2a03.lls.s -o out/rp2a03.o

out/rp2c02.o: out/rp2c02.lls.s
	clang -O3 -c out/rp2c02.lls.s -o out/rp2c02.o

out/debugger.o: gui/debugger.c gui/debugger.h
	mkdir -p out
	clang -O3 -c gui/debugger.c $(GLINCS) -o out/debugger.o

out/nes.o: nes.c gui/debugger.h
	mkdir -p out
	clang -O3 -c nes.c $(GLINCS) $(ALINCS) $(ZINCS) -o out/nes.o

out/miniunz.o: minizip/miniunz.c
	mkdir -p out
	clang -O3 -c minizip/miniunz.c $(ZINCS) -o out/miniunz.o

out/unzip.o: minizip/unzip.c
	mkdir -p out
	clang -O3 -c minizip/unzip.c $(ZINCS) -o out/unzip.o

#out/iowin32.o: minizip/iowin32.c
#	mkdir -p out
#	clang -O3 -c minizip/iowin32.c $(ZINCS) -o out/iowin32.o

out/ioapi.o: minizip/ioapi.c
	mkdir -p out
	clang -O3 -c minizip/ioapi.c $(ZINCS) -o out/ioapi.o

#out/iowin32.o out/iowin32.o
nes: out/nes.o out/debugger.o out/rp2a03.o out/miniunz.o out/ioapi.o out/unzip.o out/rp2c02.o
	clang -g3 -O3 $(PROF_FLAGS) -Wl,-Map,nes.map $(TV_DECODER) out/nes.o out/debugger.o out/rp2a03.o out/rp2c02.o out/miniunz.o out/ioapi.o out/unzip.o $(ALLIBS) $(GLLIBS) $(ZLIB) -lpthread -o nes.exe
