CXX = g++
CPPFLAGS =  -ltbb
LDFLAGS =  -lpng
CXXFLAGS = -std=c++11 -O3 -lpthread


NVCC = nvcc
NVCCFLAGS = -Xptxas="-v" -arch=sm_61 -rdc=true
NVCCTARGETS = MyChaos_with_cuda


%: %.cc
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $(CXXFLAGS) $^ -o $@

$(NVCCTARGETS): %: %.cu
	$(NVCC) $(NVCCFLAGS) $(CPPFLAGS) $(LDFLAGS) $(CXXFLAGS) $^ -o $@ 


.PHONY: clean
clean:
	rm -f $(TARGETS) $(TARGETS:=.o)

png:
	rm -rf ./pic/*png

run:
	srun -N1 -n1 -c12 ./MyChaos_with_pthread

gpu:
	srun -n1 -c12 --gres=gpu:2 ./$(NVCCTARGETS)
