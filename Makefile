
CXX=g++
CXXFLAGS=-g -std=c++17 -Og
LDFLAGS=-rdynamic
LIBS=-lpthread
INCS=
DEFS=
ifeq ($(USE_STD_SHARED_PTR),1)
DEFS+=-DUSE_STD_SHARED_PTR
endif

a.out:
	$(CXX) -o lgc.o -c lgc.cpp $(CXXFLAGS) $(INCS) $(DEFS)
	$(CXX) -o main.o -c main.cpp $(CXXFLAGS) $(INCS) $(DEFS)
	$(CXX) -o a.out main.o lgc.o $(LDFLAGS) $(LIBS)

clean:
	rm -rf a.out *.o