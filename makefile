server_test:server.o thread_pool.o
	g++ -o server_test server.o thread_pool.o -pthread -std=c++1z

server.o:server.cpp
	g++ -c server.cpp

thread_pool.o:thread_pool.cpp
	g++ -c thread_pool.cpp
