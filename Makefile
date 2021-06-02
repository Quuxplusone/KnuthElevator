go: Makefile cxx14.cpp xoshiro256ss.h
	$(CXX) -std=c++14 -O2 -Wall -Wextra -pedantic $(CXXFLAGS) cxx14.cpp -o go

clean:
	rm -f go

.PHONY: clean go
