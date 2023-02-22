
INCS=
DEFS=
CXX=g++
CXXFLAGS=-g -std=c++17 -Og
LDFLAGS=-rdynamic
LIBS=-lpthread

a.out:
	$(CXX) -o lgc.o -c lgc.cpp $(CXXFLAGS) $(INCS) $(DEFS)
	$(CXX) -o main.o -c main.cpp $(CXXFLAGS) $(INCS) $(DEFS)
	$(CXX) -o a.out main.o lgc.o $(LDFLAGS) $(LIBS)

clean:
	rm -rf a.out *.o