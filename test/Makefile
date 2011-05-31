default: gpumeminfo ocl_query

gpumeminfo:
	gcc -o gpumeminfo gpumeminfo.c \
	-L/usr/local/cuda/lib64 \
	-I/usr/local/cuda/include -lcuda -lcudart

ocl_query: ocl_query.cpp
	gcc -o ocl_query ocl_query.cpp \
	-I/usr/local/cuda/include -lOpenCL

clean:
	rm -f gpumeminfo ocl_query
