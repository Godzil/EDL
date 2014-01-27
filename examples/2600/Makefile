all: 2600

GLLIBS= -I../../../glfw-3.0.4/include -L../../../glfw-3.0.4/lib-mingw -lglfw3 -lglu32 -lopengl32 -lgdi32
ALLIBS= -I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include /c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) 2600.exe

out/m6507_main.lls: ../../chips/Accurate/m6507.edl
	mkdir -p out
	$(EDL) -O2 -s MAIN_ ../../chips/Accurate/m6507.edl >out/m6507_main.lls

out/m6507_main.lls.s: out/m6507_main.lls
	llc out/m6507_main.lls

2600: 2600.c out/m6507_main.lls.s 
	gcc -O3 -g 2600.c out/m6507_main.lls.s $(ALLIBS) $(GLLIBS) -o 2600.exe
