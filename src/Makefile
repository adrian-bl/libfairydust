default: objdir libfairydust.so


libfairydust.so:
	gcc -fPIC -rdynamic -c -Wunused-value -Wunused-variable -Wall -I/usr/local/cuda/include -I$(CUDA_INSTALL_PATH)/include -O0 \
	-D_COMPILED_AT=`date +%s` -D_GIT_LOG_COUNT=`git log | grep -c ^commit` fairydust.c
	gcc -shared -Wl,-soname,libfairydust.so.0 -o obj/libfairydust.so.0 fairydust.o -lc -ldl
	rm *.o

objdir:
	rm -rf obj/
	mkdir obj/

clean:
	rm -rf obj/
	rm -f *.so *.o

