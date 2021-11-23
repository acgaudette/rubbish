all: demo

clean:
	rm -rf demo *.o *vert*.h *frag*.h

demo: demo.o rubbish.a
	clang $^ \
	-lpthread -lm -ldl -lX11 \
	-L. -l:cimgui.so \
	-rpath . \
	-o $@
demo.o: main.c vert.h frag.h
	clang -Werror -O0 -ggdb \
	-I. \
	-c $< -o $@

vert.h: vert_user.glsl
	xxd -i $< > $@
frag.h: frag_user.glsl
	xxd -i $< > $@
