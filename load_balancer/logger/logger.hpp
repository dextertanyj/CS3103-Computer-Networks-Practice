#ifndef HTTPS_PROXY_LOGGER_HPP_
#define HTTPS_PROXY_LOGGER_HPP_

#include <fstream>
#include <string>

enum Level {DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3, FATAL = 4, DISABLED = 5};

class Logger {
  public:
    explicit Logger(std::string);
    void close();
    void set_logging_level(Level);
    void write_debug(std::string, std::string = "");
    void write_info(std::string, std::string = "");
    void write_warn(std::string, std::string = "");
    void write_error(std::string, std::string = "");
    void write_fatal(std::string, std::string = "");

  protected:
    void write(Level, std::string);

  private:
    std::ofstream log_file;
    Level log_level;

    static std::string current_timestamp();
    static std::string level_to_string(Level);
};

#endif  // HTTPS_PROXY_LOGGER_HPP_
