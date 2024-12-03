CXX = g++
CXXFLAGS = -Wall -g

SOURCES = $(wildcard *.cpp) $(wildcard *.hpp)
EXECUTABLE = sort.out

all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

clean:
	rm -f $(EXECUTABLE)