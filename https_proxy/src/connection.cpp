#include "connection.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <unordered_map>

#include <boost/regex.hpp>

#include "context.hpp"

static boost::regex STANDARD_REQUEST = boost::regex("^[A-Z]+ (\\S)+ HTTP\\/\\S+\\r\\n(\\S+:(\\S| )+\\r\\n)*\\r\\n$");
static boost::regex REQUEST_LINE = boost::regex("^CONNECT (?<hostname>[^:]+)(?<port>:\\S+)? HTTP/(?<version>\\S+)\\r\\n");

#define CONNECTION_ESTABLISHED_LENGTH 39
#define BAD_REQUEST_LENGTH 28
#define FORBIDDEN_LENGTH 26
#define NOT_FOUND_LENGTH 26
#define METHOD_NOT_ALLOWED_LENGTH 35
#define VERSION_NOT_SUPPORTED_LENGTH 43
#define BAD_GATEWAY_LENGTH 28

static const char *const HTTP_CONNECTION_ESTABLISHED = "HTTP/1.%d 200 Connection established\r\n\r\n";
static const char *const HTTP_BAD_REQUEST = "HTTP/1.%d 400 Bad Request\r\n\r\n";
static const char *const HTTP_FORBIDDEN = "HTTP/1.%d 403 Forbidden\r\n\r\n";
static const char *const HTTP_NOT_FOUND = "HTTP/1.%d 404 Not Found\r\n\r\n";
static const char *const HTTP_METHOD_NOT_ALLOWED = "HTTP/1.%d 405 Method Not Allowed\r\n\r\n";
static const char *const HTTP_VERSION_NOT_SUPPORTED = "HTTP/1.1 505 HTTP Version Not Supported\r\n\r\n";
static const char *const HTTP_BAD_GATEWAY = "HTTP/1.%d 502 Bad Gateway\r\n\r\n";

Connection::Connection(std::shared_ptr<boost::asio::ip::tcp::socket> client_socket, std::string header) {
  this->client_socket = client_socket;
  this->total_size = 0;

  if (!validate_header(header)) {
    this->write_error_to_client(HTTP_BAD_REQUEST, BAD_REQUEST_LENGTH, header);
    throw BadRequestException("Bad request");
  }
  if (!(header.find("CONNECT") == 0)) {
    this->write_error_to_client(HTTP_METHOD_NOT_ALLOWED, METHOD_NOT_ALLOWED_LENGTH, header);
    throw UnsupportedHTTPMethod("HTTP method not supported");
  }
  boost::smatch request_line_match;
  if (!boost::regex_search(header, request_line_match, REQUEST_LINE)) {
    this->write_error_to_client(HTTP_BAD_REQUEST, BAD_REQUEST_LENGTH, header);
    throw BadRequestException("Request line format error");
  }
  this->hostname = request_line_match["hostname"].str();
  if (request_line_match["version"].str() == HTTP_VERSION_1) {
    this->version = 1;
  } else if (request_line_match["version"].str() == HTTP_VERSION_0) {
    this->version = 0;
  } else {
    this->write_error_to_client(HTTP_VERSION_NOT_SUPPORTED, VERSION_NOT_SUPPORTED_LENGTH, header);
    throw UnsupportedHTTPVersionException("HTTP version unsupported");
  }
  if (request_line_match["port"].str() != "") {
    this->port = atoi(request_line_match["port"].str().substr(1).c_str());
  } else {
    this->port = HTTPS_PORT;
  }

  if (ctx.blacklist.is_blocked(this->hostname)) {
    this->write_error_to_client(HTTP_FORBIDDEN, FORBIDDEN_LENGTH, header);
    throw BlockedException("Website blocked: " + this->hostname);
  }

  int options_token = header.find("\r\n");
  std::string raw_options = header.substr(options_token + 2);
  raw_options.pop_back();
  raw_options.pop_back();
  set_options(raw_options);
}

Connection::~Connection() {
  if (this->has_telemetry()) {
    int duration = std::chrono::duration_cast<std::chrono::milliseconds>(this->end_time - this->start_time).count();
    std::string telemetry = "Hostname: " + this->hostname + ", Size: " +
    std::to_string(this->total_size) + " bytes, Time: " + std::to_string(duration/(1000.0)) + " sec";
    ctx.logger.write_info(telemetry);
    if (ctx.telemetry) {
      printf("%s\n", telemetry.c_str());
    }
  }
  free(this->client_buffer);
  free(this->server_buffer);
}

std::shared_ptr<Connection> Connection::create(std::shared_ptr<boost::asio::ip::tcp::socket> client_socket, std::string header) {
  return std::shared_ptr<Connection>(new Connection(client_socket, header));
}

void Connection::handle_connection(std::string initial_data) {
  std::shared_ptr<boost::asio::ip::tcp::socket> client_socket = this->client_socket;
  std::shared_ptr<boost::asio::ip::tcp::socket> server_socket = std::make_shared<boost::asio::ip::tcp::socket>(ctx.ctx);
  this->server_socket = server_socket;
  boost::asio::ip::tcp::endpoint server_endpoint;
  try {
    server_endpoint = Connection::resolve(this->hostname + ".", std::to_string(this->port));
  } catch (NameResolutionError &e) {
    std::string message = e.what();
    ctx.logger.write_warn("Failed to resolve: " + this->hostname + "|" + message, "Connection::handle_connection");
    this->write_error_to_client(HTTP_NOT_FOUND, NOT_FOUND_LENGTH, this->version);
    client_socket->close();
    server_socket->close();
    return;
  }
  ctx.logger.write_info("Connecting to: " + hostname + ":" + std::to_string(port));
  this->start();
  try {
    server_socket->open(server_endpoint.protocol());
    server_socket->connect(server_endpoint);
  } catch (boost::system::system_error &e) {
    std::string message = e.what();
    ctx.logger.write_error("Failed to connect: " + this->hostname + "|" + message, "Connection::handle_connection");
    this->write_error_to_client(HTTP_BAD_GATEWAY, BAD_GATEWAY_LENGTH, this->version);
    client_socket->close();
    server_socket->close();
    return;
  }
  char message[CONNECTION_ESTABLISHED_LENGTH + 1] = {0};
  snprintf(message, CONNECTION_ESTABLISHED_LENGTH + 1,
    HTTP_CONNECTION_ESTABLISHED, this->version);
  try {
    boost::asio::write(*client_socket, boost::asio::buffer(message, CONNECTION_ESTABLISHED_LENGTH));
  } catch (boost::system::system_error &e) {
    ctx.logger.write_error("Write failed: " + std::string(e.what()), "Connection::handle_connection");
    return;
  }

  client_socket->async_receive(boost::asio::buffer(this->client_buffer, BUFFER_SIZE),
    boost::bind(&Connection::handle_read,
      shared_from_this(),
      client_socket, server_socket,
      this->client_buffer,
      false,
      boost::asio::placeholders::bytes_transferred, boost::asio::placeholders::error));
  server_socket->async_receive(boost::asio::buffer(this->server_buffer, BUFFER_SIZE),
    boost::bind(&Connection::handle_read,
      shared_from_this(),
      server_socket, client_socket,
      this->server_buffer,
      true,
      boost::asio::placeholders::bytes_transferred, boost::asio::placeholders::error));
  return;
}

std::shared_ptr<Connection> Connection::shared_ptr() {
  return shared_from_this();
}

bool Connection::has_telemetry() {
  return this->start_time != std::chrono::system_clock::time_point() && this->end_time != std::chrono::system_clock::time_point();
}

void Connection::set_options(std::string& options) {
  while (options.find("\r\n") != std::string::npos) {
    int key_token = options.find(":");
    int next_token = options.find("\r\n");
    std::string key = options.substr(0, key_token - 1);
    std::string value = options.substr(key_token + 1, next_token - key_token + 1);
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    (this->options)[key] = value;
    options = options.substr(next_token + 2);
  }
}

void Connection::handle_read(
  std::shared_ptr<boost::asio::ip::tcp::socket> read,
  std::shared_ptr<boost::asio::ip::tcp::socket> write,
  char* buffer,
  bool record_transfer,
  size_t bytes_transferred,
  const boost::system::error_code &error
) {
  boost::lock_guard<boost::mutex> lock(this->lock);
  if ((error == boost::asio::error::eof) ||
  (error == boost::asio::error::connection_reset) ||
  (error == boost::asio::error::connection_aborted)) {
    ctx.logger.write_debug("Read failed: " + error.message(), "Connection::handle_read");
    this->end();
    read->close();
    write->close();
    return;
  }
  if (!write->is_open()) {
    return;
  }
  try {
    boost::asio::write(*write, boost::asio::buffer(buffer, bytes_transferred));
  } catch (boost::system::system_error &e) {
    ctx.logger.write_warn("Write failed: " + std::string(e.what()), "Connection::handle_read");
    return;
  }
  if (record_transfer) {
    this->record_payload(bytes_transferred);
  }
  read->async_read_some(boost::asio::buffer(buffer, BUFFER_SIZE),
    boost::bind(&Connection::handle_read,
      shared_from_this(),
      read, write, buffer, record_transfer,
      boost::asio::placeholders::bytes_transferred, boost::asio::placeholders::error));
}

void Connection::start() {
  this->start_time = std::chrono::system_clock::now();
}

void Connection::record_payload(int payload_size) {
  this->total_size += payload_size;
}

void Connection::end() {
  if (this->start_time == std::chrono::system_clock::time_point()) {
    ctx.logger.write_error("Attempting to closed unstarted connection.");
  }
  if (this->end_time == std::chrono::system_clock::time_point()) {
    this->end_time = std::chrono::system_clock::now();
  }
}

void Connection::write_error_to_client(const char *const message, int length, int version) {
  char buffer[length + 1] = {0};
  snprintf(buffer, length + 1, message, version);
  try {
    boost::asio::write(*(this->client_socket), boost::asio::buffer(buffer, length));
  } catch (boost::system::system_error &e) {
    ctx.logger.write_error("Write failed: " + std::string(e.what()), "Connection::write_error_to_client");
  }
}

void Connection::write_error_to_client(const char *const message, int length, std::string &header) {
  int version = 1;
  if (size_t token = header.find("HTTP/1.") ) {
    if (header.size() > token + 8) {
      version = atoi(header.substr(token + 7, 1).c_str());
    }
  }
  this->write_error_to_client(message, length, version);
}

bool Connection::validate_header(std::string& header) {
  return boost::regex_match(header, STANDARD_REQUEST);
}

boost::asio::ip::tcp::endpoint Connection::resolve(std::string hostname, std::string port) {
  boost::lock_guard<boost::mutex> guard(ctx.resolver_mutex);
  try {
    boost::asio::ip::tcp::resolver::query query(hostname, port);
    boost::asio::ip::tcp::resolver::iterator results = ctx.resolver.resolve(query);
    return results->endpoint();
  } catch (boost::system::system_error &e) {
    throw NameResolutionError(e.what());
  }
}
