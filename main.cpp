#include <csignal>
#include <iostream>

#include "context.hpp"
#include "server.hpp"

int main(int argc, char * argv[]) {
  if (argc != 2) {
    std::cout << "Usage: ./proxy <port>";
    return 255;
  }
  Server server = Server(atoi(argv[1]));
  server.listen();
  logger.close();
  return 0;
}
