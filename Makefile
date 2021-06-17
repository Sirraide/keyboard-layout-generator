src = $(wildcard src/*.cc src/*.c)
headers = $(wildcard src/*.h)
LIBS = -lstdc++ -lfmt
CXX=clang
CXXFLAGS = -Wall -Wextra -Wundef -std=c++2a
gen: $(src) $(headers)
	$(CXX) $(CXXFLAGS) $(src) $(LIBS) -o $@ -O3

.PHONY: debug, clean

debug:
	$(CXX) $(CXXFLAGS) $(src) $(LIBS) -o gen -g

clean:
	rm -f gen