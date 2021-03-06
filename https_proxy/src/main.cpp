#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "context.hpp"
#include "server.hpp"

int main(int argc, char * argv[]) {
  if (argc < 2 || argc > 5) {
    std::cout << "Usage: ./proxy PORT [TELEMETRY_FLAG [PATH_TO_BLACKLIST]]`" << std::endl;
    return 1;
  }
  if (argc >= 3) {
    std::string telemetry_flag = std::string(argv[2]);
    if (telemetry_flag == "0") {
      ctx.telemetry = false;
    } else if (telemetry_flag == "1") {
      ctx.telemetry = true;
    } else {
      std::cout << "Invalid options\n" << "Telemetry = 0 (Disabled) | 1 (Enabled)" << std::endl;
      return 2;
    }
  }
  if (argc == 5) {
    std::string level = std::string(argv[4]);
    if (level == "debug") {
      ctx.logger.set_logging_level(DEBUG);
    } else if (level == "info") {
      ctx.logger.set_logging_level(INFO);
    } else if (level == "warn") {
      ctx.logger.set_logging_level(WARN);
    } else if (level == "error") {
      ctx.logger.set_logging_level(ERROR);
    } else {
      std::cout << "Invalid options\n" << "Logging = debug | info | warn | error" << std::endl;
      return 2;
    }
  }
  if (argc >= 4 && access(argv[3], F_OK) == 0) {
    std::ifstream blacklist_file;
    blacklist_file.open(argv[3], std::ios::in);
    std::unique_ptr<std::vector<std::string>> entries = std::make_unique<std::vector<std::string>>();
    std::string buffer;
    while (std::getline(blacklist_file, buffer)) {
      if (buffer.size() > 0) {
        entries->push_back(buffer);
      }
    }
    blacklist_file.close();
    ctx.blacklist.add_entries(std::move(entries));
  } else if (argc >= 4) {
    ctx.logger.write_info("Blacklist file not found: " + std::string(argv[3]));
    std::cout << "Specified file not found: " << std::string(argv[3]) << std::endl;
    return 3;
  }
  std::shared_ptr<Server> server = Server::create(atoi(argv[1]));
  server->listen();
  ctx.logger.write_info("Gracefully stopped proxy.");
  ctx.logger.close();
  return 0;
}