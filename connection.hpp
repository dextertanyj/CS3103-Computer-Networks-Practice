#ifndef HTTPS_PROXY_CONNECTION_HPP_
#define HTTPS_PROXY_CONNECTION_HPP_

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#define BUFFER_SIZE 8192
#define CONNECTION_ESTABLISHED_LENGTH 41
#define HTTPS_PORT 443
#define HTTP_VERSION_1 "1.1"
#define HTTP_VERSION_0 "1.0"

struct BadRequestException : public std::runtime_error {
  using runtime_error::runtime_error;
};

struct UnsupportedHTTPVersionException : public std::runtime_error {
  using runtime_error::runtime_error;
};

struct NameResolutionError : public std::runtime_error {
  using runtime_error::runtime_error;
};

class Connection : public std::enable_shared_from_this<Connection> {
  public:
    ~Connection();
    static std::shared_ptr<Connection> create(boost::asio::ip::tcp::socket*, std::string);
    void handle_connection(std::string);

    std::shared_ptr<Connection> shared_ptr();
    boost::asio::ip::tcp::socket* get_client_socket();
    boost::asio::ip::tcp::socket* get_server_socket();
    std::string get_hostname();
    int get_port();
    int get_version();
    std::string get_option(std::string);

  private:
    // Connection information
    boost::asio::ip::tcp::socket* client_socket;
    boost::asio::ip::tcp::socket* destination_socket;
    std::string hostname;
    int port;
    int version;
    std::unordered_map<std::string, std::string> options;

    // Connection statistics
    std::chrono::_V2::system_clock::time_point start_time;
    std::chrono::_V2::system_clock::time_point end_time;
    int total_size;
    
    boost::mutex lock;
    char* client_buffer = reinterpret_cast<char*>(malloc(sizeof(char) * BUFFER_SIZE));
    char* server_buffer = reinterpret_cast<char*>(malloc(sizeof(char) * BUFFER_SIZE));

    Connection(boost::asio::ip::tcp::socket*, std::string);
    void set_options(std::string&);
    void handle_read(boost::asio::ip::tcp::socket*, boost::asio::ip::tcp::socket*,
      char*, size_t, const boost::system::error_code);
    void start();
    void record_payload(int);
    void end();

    static bool validate_header(std::string&);
    static boost::asio::ip::tcp::endpoint resolve(std::string, std::string);
};

#endif  // HTTPS_PROXY_CONNECTION_HPP_
