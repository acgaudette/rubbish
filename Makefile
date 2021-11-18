all: demo rubbish.a

clean:
	rm -rf demo rubbish.a *.o obj vert.h frag.h post_vert.h post_frag.h

demo: demo.o rubbish.a
	clang $^ \
	-lpthread -lm -ldl -lX11 \
	-L. -l:cimgui.so \
	-rpath . \
	-o $@
demo.o: demo.c mesh.h
	clang -Werror -O0 -ggdb \
	-I. -I.. \
	-c $< -o $@

rubbish.a: rubbish.o gl3w.o obj
	ar rcs $@ $< gl3w.o obj/*.o

obj: libglfw3.a
	mkdir obj && cd obj
	ar xv $< --output $@

rubbish.o: lib.c bin.h vert.h frag.h post_vert.h post_frag.h
	clang -Werror -O0 -ggdb \
	-DDEBUG -DRUBBISH_IMGUI \
	-I. \
	-c $< -o $@
gl3w.o: ./gl3w/src/gl3w.c
	clang -I./gl3w/include -c $< -o $@

vert.h: vert.glsl
	xxd -i $< > $@
frag.h: frag.glsl
	xxd -i $< > $@
post_vert.h: post_vert.glsl
	xxd -i $< > $@
post_frag.h: post_frag.glsl
	xxd -i $< > $@
