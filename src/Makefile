default: envmode

envmode: objdir cuvisible.o common.o
	gcc -shared -Wl,-soname,libfairydust.so.0 -o obj/libfairydust.so.0 cuvisible.o common.o -lc -ldl
	rm *.o

cudamode: objdir fairydust.o common.o
	gcc -shared -Wl,-soname,libfairydust.so.0 -o obj/libfairydust.so.0 fairydust.o common.o -lc -ldl
	rm *.o

common.o:
	gcc -fPIC -rdynamic -c -Wunused-value -Wunused-variable -Wall \
	-D_COMPILED_AT=`date +%s` -D_GIT_LOG_COUNT=`git log | grep -c ^commit` common.c

fairydust.o:
	gcc -fPIC -rdynamic -c -Wunused-value -Wunused-variable -Wall -I/usr/local/cuda/include -I$(CUDA_INSTALL_PATH)/include \
	fairydust.c

cuvisible.o:
	gcc -fPIC -rdynamic -c -Wunused-value -Wunused-variable -Wall \
	cuvisible.c


objdir:
	rm -rf obj/
	mkdir obj/

clean:
	rm -rf obj/
	rm -f *.so *.o

