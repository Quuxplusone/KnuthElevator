go: Makefile cxx14.cpp xoshiro256ss.h
	$(CXX) -std=c++14 -O2 -Wall -Wextra -pedantic $(CXXFLAGS) cxx14.cpp -o go

spiders: Makefile spiders.cpp
	$(CXX) -std=c++20 -O2 -Wall -Wextra -pedantic $(CXXFLAGS) spiders.cpp -o spiders

clean:
	rm -f go spiders

.PHONY: clean go
