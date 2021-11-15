#include "logger.hpp"

#include <time.h>
#include <string>

#include <regex>

static std::regex CR = std::regex("\r");
static std::string CR_REPLACEMENT = R"(\\r)";
static std::regex LF = std::regex("\n");
static std::string LF_REPLACEMENT = R"(\\n)";

Logger::Logger(std::string path) {
  this->log_file.open(path, std::ios::app);
  this->log_level = DISABLED;
}

void Logger::close() {
  this->log_file.close();
}

void Logger::set_logging_level(Level lvl) {
  this->log_level = lvl;
}

void Logger::write(Level lvl, std::string message) {
  std::string trimmed = std::regex_replace(message, CR, CR_REPLACEMENT);
  trimmed = std::regex_replace(trimmed, LF, LF_REPLACEMENT);
  std::string timestamp = Logger::current_timestamp();
  std::string lvl_string = Logger::level_to_string(lvl);
  std::string formatted_message = timestamp + "|" + lvl_string + "|" + trimmed + "\n";
  this->log_file << formatted_message << std::flush;
}

void Logger::write_debug(std::string message, std::string function) {
  if (this->log_level > 0) {
    return;
  }
  if (function != "") {
    message = function + "|" + message;
  }
  this->write(DEBUG, message);
}

void Logger::write_info(std::string message, std::string function) {
  if (this->log_level > 1) {
    return;
  }
  if (function != "") {
    message = function + "|" + message;
  }
  this->write(INFO, message);
}

void Logger::write_warn(std::string message, std::string function) {
  if (this->log_level > 2) {
    return;
  }
  if (function != "") {
    message = function + "|" + message;
  }
  this->write(WARN, message);
}

void Logger::write_error(std::string message, std::string function) {
  if (this->log_level > 4) {
    return;
  }
  if (function != "") {
    message = function + "|" + message;
  }
  this->write(ERROR, message);
}

void Logger::write_fatal(std::string message, std::string function) {
  if (this->log_level > 4) {
    return;
  }
  if (function != "") {
    message = function + "|" + message;
  }
  this->write(FATAL, message);
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
    case FATAL:
      return "FATAL";
    default:
      return "";
  }
}
