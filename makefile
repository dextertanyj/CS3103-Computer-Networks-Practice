CC=g++
CFLAGS = -Wall
LIBS=-lpthread -lboost_regex -lboost_thread
TARGET=proxy

proxy: main.o server.o connection.o context.o logger.o 
	$(CC) $(CFLAGS) -o proxy main.o server.o connection.o context.o logger.o $(LIBS)

main.o: main.cpp server.hpp context.hpp
	$(CC) $(CFLAGS) -c main.cpp $(LIBS)

server.o: server.cpp server.hpp connection.hpp context.hpp
	$(CC) $(CFLAGS) -c server.cpp $(LIBS)

connection.o: connection.cpp connection.hpp context.hpp
	$(CC) $(CFLAGS) -c connection.cpp $(LIBS)

context.o: context.cpp context.hpp logger.hpp
	$(CC) $(CFLAGS) -c context.cpp

logger.o: logger.cpp logger.hpp
	$(CC) $(CFLAGS) -c logger.cpp

clean:
	$(RM) proxy *.o
