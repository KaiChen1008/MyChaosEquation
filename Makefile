CXX = g++
CPPFLAGS =
LDFLAGS = 
CXXFLAGS = -std=c++11 -O3 -lpthread

%: %.cc
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $(CXXFLAGS) $^ -o $@


.PHONY: clean
clean:
	rm -f $(TARGETS) $(TARGETS:=.o)
