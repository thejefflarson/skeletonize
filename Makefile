CFLAGS := -frounding-math -Wall -pedantic -ggdb
CC=g++
INCLUDES := -I/usr/local/include
LIBS :=  -L/usr/local/lib -lCGAL -lCGAL_Core -lboost_thread-mt -lgdal -lgmp -lmpfr

skeleton: skeleton.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -o skeleton skeleton.cpp $(LIBS)

clean:
	rm -fr *.o skeleton
