

bin/dijkstra: include/graph-mutable-va.hpp test/dijkstra.cpp
	g++ -I./include -O3 -o $@ $^ -g -lpthread -std=c++2a
