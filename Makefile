CXX=clang++-mp-3.3
CXXFLAGS=-I. -std=c++11 -stdlib=libc++ -O3 -march=native
LIBS=-framework Cocoa -framework OpenGL -lglfw -framework IOKit
gol: gol.cpp scopeguard.hpp
	$(CXX) -o $@ $< $(CXXFLAGS) $(LIBS)
