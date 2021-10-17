CC=g++
CFLAGS=-Wall -O3
LIBS=-lpthread -lboost_regex -lboost_thread
TARGET=proxy

proxy: main.o server.o connection.o context.o blacklist.o logger.o
	$(CC) $(CFLAGS) -o proxy main.o server.o connection.o context.o blacklist.o logger.o $(LIBS)

main.o: src/main.cpp src/server.hpp src/context.hpp
	$(CC) $(CFLAGS) -c src/main.cpp

server.o: src/server.cpp src/server.hpp src/connection.hpp src/context.hpp 
	$(CC) $(CFLAGS) -c src/server.cpp

connection.o: src/connection.cpp src/connection.hpp src/context.hpp
	$(CC) $(CFLAGS) -c src/connection.cpp

context.o: src/context.cpp src/context.hpp src/blacklist.hpp src/logger/logger.hpp
	$(CC) $(CFLAGS) -c src/context.cpp

blacklist.o: src/blacklist.cpp src/blacklist.hpp
	$(CC) $(CFLAGS) -c src/blacklist.cpp

logger.o: src/logger/logger.cpp src/logger/logger.hpp
	$(CC) $(CFLAGS) -c src/logger/logger.cpp

.PHONY: clean

clean:
	$(RM) proxy *.o
