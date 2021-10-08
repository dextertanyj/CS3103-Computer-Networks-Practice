#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "context.hpp"
#include "server.hpp"

int main(int argc, char * argv[]) {
  if (argc != 4) {
    std::cout << "Usage: ./proxy <port> <enable_telemetry> <path_to_blacklist>";
    return 255;
  }
  switch (atoi(argv[2])) {
    case 0:
      ctx.telemetry = false;
      break;
    case 1:
      ctx.telemetry = true;
      break;
    default:
      std::cout << "Invalid arguments";
      return 1;
  }
  if (access(argv[3], F_OK) == 0) {
    std::ifstream blacklist_file;
    blacklist_file.open(argv[3], std::ios::in);
    std::vector<std::string> *entries = new std::vector<std::string>();
    std::string buffer;
    while (std::getline(blacklist_file, buffer)) {
      if (buffer.size() > 0) {
        entries->push_back(buffer);
      }
    }
    blacklist_file.close();
    ctx.blacklist.add_entries(entries);
  } else {
    ctx.logger.write_info("Blacklist file not found: " + std::string(argv[3]));
  }
  Server server = Server(atoi(argv[1]));
  server.listen();
  ctx.logger.close();
  return 0;
}