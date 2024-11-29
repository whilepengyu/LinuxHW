CXX = g++
CXXFLAGS = -Wall -g

SOURCES = $(wildcard *.cpp)
EXECUTABLE = myls

all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@

clean:
	rm -f $(EXECUTABLE)