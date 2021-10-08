#ifndef HTTPS_PROXY_LOGGER_HPP_
#define HTTPS_PROXY_LOGGER_HPP_

#include <fstream>
#include <string>

enum Level {DEBUG, INFO, WARN, ERROR};

class Logger {
  public:
    explicit Logger(std::string);
    void write_debug(std::string);
    void write_info(std::string);
    void write_warn(std::string);
    void write_error(std::string);
    void close();

  protected:
    void write(Level, std::string);

  private:
    std::ofstream logFile;

    static std::string current_timestamp();
    static std::string level_to_string(Level);
};

#endif  // HTTPS_PROXY_LOGGER_HPP_
