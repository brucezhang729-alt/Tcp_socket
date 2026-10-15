# TCP echo client/server build rules

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic

TARGETS := tcp_server tcp_client

.PHONY: all clean rebuild

all: $(TARGETS)

tcp_server: tcp_server.cpp
	$(CXX) $(CXXFLAGS) -pthread $< -o $@

tcp_client: tcp_client.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)

rebuild: clean all
