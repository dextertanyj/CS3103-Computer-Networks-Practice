CC=g++
CFLAGS=-Wall -O3
LIBS=-lpthread -lboost_regex -lboost_thread
TARGET=proxy

proxy: main.o server.o connection.o context.o blacklist.o logger.o
	$(CC) $(CFLAGS) -o proxy main.o server.o connection.o context.o blacklist.o logger.o $(LIBS)

main.o: main.cpp server.hpp context.hpp
	$(CC) $(CFLAGS) -c main.cpp

server.o: server.cpp server.hpp connection.hpp context.hpp
	$(CC) $(CFLAGS) -c server.cpp

connection.o: connection.cpp connection.hpp context.hpp
	$(CC) $(CFLAGS) -c connection.cpp

context.o: context.cpp context.hpp blacklist.hpp logger.hpp
	$(CC) $(CFLAGS) -c context.cpp

blacklist.o: blacklist.cpp blacklist.hpp
	$(CC) $(CFLAGS) -c blacklist.cpp

logger.o: logger.cpp logger.hpp
	$(CC) $(CFLAGS) -c logger.cpp

clean:
	$(RM) proxy *.o
