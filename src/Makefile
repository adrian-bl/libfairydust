default: envmode

envmode: objdir cuvisible.o common.o
	gcc -shared -Wl,-soname,libfairydust.so.0 -o obj/libfairydust.so.0 cuvisible.o common.o -lc -ldl
	rm *.o

cudamode: objdir fairydust.o common.o
	gcc -shared -Wl,-soname,libfairydust.so.0 -o obj/libfairydust.so.0 fairydust.o common.o -lc -ldl
	rm *.o

atidust: objdir amdati.o common.o
	gcc -shared -Wl,-soname,libatidust.so.0 -o obj/libatidust.so.0 amdati.o common.o -lc -ldl
	rm *.o

32bit: objdir fake.o
	gcc -m32 -shared -Wl,-soname,libatidust.so.0   -o obj/libatidust.so.0   fake.o -lc -ldl
	gcc -m32 -shared -Wl,-soname,libfairydust.so.0 -o obj/libfairydust.so.0 fake.o -lc -ldl

amdati.o:
	gcc -fPIC -rdynamic -c -Wunused-value -Wunused-variable -Wall -I/usr/local/cuda/include -I$(CUDA_INSTALL_PATH)/include \
	amdati.c

common.o:
	gcc -fPIC -rdynamic -c -Wunused-value -Wunused-variable -Wall \
	-D_COMPILED_AT=`date +%s` -D_GIT_LOG_COUNT=`git log | grep -c ^commit` common.c

fairydust.o:
	gcc -fPIC -rdynamic -c -Wunused-value -Wunused-variable -Wall -I/usr/local/cuda/include -I$(CUDA_INSTALL_PATH)/include \
	fairydust.c

cuvisible.o:
	gcc -fPIC -rdynamic -c -Wunused-value -Wunused-variable -Wall \
	cuvisible.c

fake.o:
	gcc -m32 -fPIC -rdynamic -c -Wunused-value -Wunused-variable -Wall \
	fake.c

objdir:
	rm -rf obj/
	mkdir obj/

clean:
	rm -rf obj/
	rm -f *.so *.o

