CFLAGS := -frounding-math
INCLUDES := -I/usr/local/include
LIBS :=  -L/usr/local/lib -L/Library/Frameworks/GDAL.framework/unix/lib/ -lCGAL -lCGAL_Core -lboost_thread-mt -lGDAL -lgmp -lmpfr

skeleton: skeleton.cpp
	g++ $(CFLAGS) $(INCLUDES) -o skeleton skeleton.cpp $(LIBS)

clean:
	rm -fr *.o skeleton
