#include "logger.hpp"

#include <time.h>
#include <string>

Logger::Logger(std::string path) {
  this->logFile.open(path, std::ios::app);
}

void Logger::write(Level lvl, std::string message) {
  std::string timestamp = Logger::current_timestamp();
  std::string lvlString = Logger::level_to_string(lvl);
  std::string formatted_message = timestamp + "|" + lvlString + "|" + message;
  this->logFile << formatted_message << std::endl;
}

void Logger::write_debug(std::string message) {
  this->write(DEBUG, message);
}

void Logger::write_info(std::string message) {
  this->write(INFO, message);
}

void Logger::write_warn(std::string message) {
  this->write(WARN, message);
}

void Logger::write_error(std::string message) {
  this->write(ERROR, message);
}

void Logger::close() {
  this->logFile.close();
}

std::string Logger::current_timestamp() {
  time_t timestamp = time(0);
  struct tm tstruct;
  char buf[80];
  localtime_r(&timestamp, &tstruct);
  strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
  return buf;
}

std::string Logger::level_to_string(Level lvl) {
  switch (lvl) {
    case INFO:
      return "INFO";
    case DEBUG:
      return "DEBUG";
    case WARN:
      return "WARN";
    case ERROR:
      return "ERROR";
    default:
      return "";
  }
}
