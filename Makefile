CXXFLAGS=-W -Wall -pedantic -std=c++14 -g -MD -MP

all: main run_unit_tests

main: main.o file.o
	$(CXX) -o $@ $^ -lPocoNet -lPocoFoundation -ljsoncpp

run_unit_tests: file_test.pass


file_test: file_test.o file.o
	$(CXX) -o $@ $^

file_test.pass: file_test
	./file_test
	touch file_test.pass

clean:
	rm -f *.o *.d *.pass main file_test

-include *.d
