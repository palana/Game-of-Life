CXX=clang++-mp-3.4
CXXFLAGS=-I. -std=c++11 -stdlib=libc++ -O3 -march=native -g
LIBS=-framework Cocoa -framework OpenGL -lglfw -framework IOKit
gol: gol.cpp scopeguard.hpp Makefile
	$(CXX) -o $@ $< $(CXXFLAGS) $(LIBS)
