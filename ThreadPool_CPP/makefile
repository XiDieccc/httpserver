all:test

test:test.cpp TaskQueue.cpp ThreadPool.cpp
	g++ TaskQueue.cpp ThreadPool.cpp test.cpp -o test -std=c++11 -lpthread

clean:
	rm -f test