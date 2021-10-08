#include "connection.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <unordered_map>

#include <boost/regex.hpp>

#include "context.hpp"

static boost::regex STANDARD_REQUEST = boost::regex("CONNECT (\\S| )+ HTTP\\/(.)+\\r\\n(\\S+:(\\S| )+\\r\\n)*\\r\\n");
static boost::regex HTTP_VERSION = boost::regex("HTTP/(\\S+)\\r\\n");
static boost::regex DESTINATION = boost::regex("CONNECT ([^:]+)(:\\S+)? HTTP/.+\\r\\n");

Connection::Connection(boost::asio::ip::tcp::socket* client_socket, std::string header) {
  this->client_socket = client_socket;
  this->total_size = 0;

  if (!validate_header(header)) {
    throw BadRequestException("Bad Request");
  }
  boost::smatch version_match;
  if (!boost::regex_search(header, version_match, HTTP_VERSION)) {
    throw BadRequestException("HTTP Version Not Found");
  }
  if (version_match.str(1) == HTTP_VERSION_1) {
    this->version = 1;
  } else if (version_match.str(1) == HTTP_VERSION_0) {
    this->version = 0;
  } else {
    throw UnsupportedHTTPVersionException("HTTP Version Unsupported");
  }
  boost::smatch target_match;
  if (!boost::regex_search(header, target_match, DESTINATION)) {
    throw BadRequestException("Target Host Not Found");
  }
  this->hostname = target_match.str(1);
  if (target_match.size() > 1) {
    this->port = atoi(target_match.str(2).substr(1).c_str());
  } else {
    this->port = HTTPS_PORT;
  }
  int options_token = header.find("\r\n");
  std::string raw_options = header.substr(options_token + 2);
  raw_options.pop_back();
  raw_options.pop_back();
  set_options(raw_options);
  logger.write_info("Connecting to: " + hostname + ":" + std::to_string(port));
}

Connection::~Connection() {
  int duration = std::chrono::duration_cast<std::chrono::milliseconds>(this->end_time - this->start_time).count();
  std::string telemetry = "Hostname: " + this->hostname + ", Size: " +
    std::to_string(this->total_size/8) + " bytes, Time: " + std::to_string(duration/(1000.0)) + " sec";
  logger.write_info(telemetry);
  std::cout << telemetry << std::endl;
  free(this->client_buffer);
  free(this->server_buffer);
}

std::shared_ptr<Connection> Connection::create(boost::asio::ip::tcp::socket* client_socket, std::string header) {
  return std::shared_ptr<Connection>(new Connection(client_socket, header));
}

void Connection::handle_connection(std::string initial_data) {
  this->start();
  boost::asio::ip::tcp::socket *client_socket = this->get_client_socket();
  boost::asio::ip::tcp::socket *destination_socket = new boost::asio::ip::tcp::socket(ctx);
  this->destination_socket = destination_socket;
  boost::asio::ip::tcp::endpoint destination;
  try {
    destination = Connection::resolve(this->get_hostname().append("."), std::to_string(this->get_port()));
  } catch (NameResolutionError &e) {
    std::string message = e.what();
    logger.write_warn("Failed to resolve: " + this->get_hostname() + "|" + message);
    client_socket->close();
    destination_socket->close();
    return;
  }
  try {
    destination_socket->open(destination.protocol());
    destination_socket->connect(destination);
  } catch (boost::system::system_error &e) {
    std::string message = e.what();
    logger.write_warn("Failed to connect: " + this->get_hostname() + "|" + message);
    client_socket->close();
    destination_socket->close();
    return;
  }
  char established[CONNECTION_ESTABLISHED_LENGTH] = {0};
  snprintf(established, CONNECTION_ESTABLISHED_LENGTH,
    "HTTP/1.%d 200 Connection established \r\n\r\n", this->get_version());
  boost::asio::write(*client_socket, boost::asio::buffer(established, 41));

  client_socket->async_receive(boost::asio::buffer(this->client_buffer, BUFFER_SIZE),
    boost::bind(&Connection::handle_read,
      shared_from_this(),
      client_socket, destination_socket,
      this->client_buffer,
      boost::asio::placeholders::bytes_transferred, boost::asio::placeholders::error));
  destination_socket->async_receive(boost::asio::buffer(this->server_buffer, BUFFER_SIZE),
    boost::bind(&Connection::handle_read,
      shared_from_this(),
      destination_socket, client_socket,
      this->server_buffer,
      boost::asio::placeholders::bytes_transferred, boost::asio::placeholders::error));
  return;
}

std::shared_ptr<Connection> Connection::shared_ptr() {
  return shared_from_this();
}

boost::asio::ip::tcp::socket* Connection::get_client_socket() {
  return this->client_socket;
}

boost::asio::ip::tcp::socket* Connection::get_server_socket() {
  return this->destination_socket;
}

std::string Connection::get_hostname() {
  return this->hostname;
}

int Connection::get_port() {
  return this->port;
}

int Connection::get_version() {
  return this->version;
}

std::string Connection::get_option(std::string key) {
  std::string key_case_insensitive = std::string();
  std::transform(key.begin(), key.end(), key_case_insensitive.begin(), ::tolower);
  if (this->options.find(key_case_insensitive) == options.end()) {
    throw "Key Not Found";
  }
  return this->options.find(key_case_insensitive)->second;
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
  boost::asio::ip::tcp::socket *read,
  boost::asio::ip::tcp::socket *write,
  char* buffer,
  size_t bytes_transferred,
  const boost::system::error_code error
) {
  boost::lock_guard<boost::mutex> lock(this->lock);
  if ((error == boost::asio::error::eof) ||
  (error == boost::asio::error::connection_reset) ||
  (error == boost::asio::error::connection_aborted)) {
    this->end();
    read->close();
    write->close();
    return;
  }
  if (!write->is_open()) {
    return;
  }
  boost::asio::write(*write, boost::asio::buffer(buffer, bytes_transferred));
  this->record_payload(bytes_transferred);
  read->async_read_some(boost::asio::buffer(buffer, BUFFER_SIZE),
    boost::bind(&Connection::handle_read,
      shared_from_this(),
      read, write, buffer,
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
    throw "Connection was not started.";
  }
  if (this->end_time == std::chrono::system_clock::time_point()) {
    this->end_time = std::chrono::system_clock::now();
  }
}

bool Connection::validate_header(std::string& header) {
  return boost::regex_match(header, STANDARD_REQUEST);
}

boost::asio::ip::tcp::endpoint Connection::resolve(std::string hostname, std::string port) {
  boost::lock_guard<boost::mutex> guard(resolver_mutex);
  try {
    boost::asio::ip::tcp::resolver::query query(hostname, port);
    boost::asio::ip::tcp::resolver::iterator results = resolver.resolve(query);
    return results->endpoint();
  } catch (boost::system::system_error &e) {
    throw NameResolutionError(e.what());
  }
}
