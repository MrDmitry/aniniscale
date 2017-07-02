CXX=g++
CPPFLAGS=-g -Wall -Werror -pedantic -std=c++11 ${CPP_EXTRA_FLAGS}
LDFLAGS=${VIPS_FLAGS}
OBJDIR:=.obj

all: aniniscale

$(OBJDIR):
	mkdir -p $@

$(OBJDIR)/Reporter.o: Reporter.cpp Reporter.hpp
	$(CXX) $(CPPFLAGS) -c Reporter.cpp -o $@

$(OBJDIR)/WorkerPool.o: WorkerPool.cpp WorkerPool.hpp Reporter.hpp
	$(CXX) $(CPPFLAGS) -c WorkerPool.cpp -o $@

aniniscale: main.cpp $(OBJDIR)/Reporter.o $(OBJDIR)/WorkerPool.o Reporter.hpp WorkerPool.hpp
	$(CXX) $(CPPFLAGS) -o $@ main.cpp $(OBJDIR)/Reporter.o $(OBJDIR)/WorkerPool.o $(LDFLAGS)

.PHONY: clean
clean:
	rm -f aniniscale $(OBJDIR)/*.o
