#include "server.hpp"

#include <string>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include "connection.hpp"
#include "context.hpp"

#define ALL_INTERFACES {0, 0, 0, 0}
#define THREAD_COUNT 7
#define END_OF_MESSAGE "\r\n\r\n"

Server::Server(int port) {
  this->thread_group = new boost::thread_group();
  uint16_t listen_port = boost::lexical_cast<uint16_t>(port);
  boost::asio::ip::address_v4 local_address = boost::asio::ip::address_v4(ALL_INTERFACES);
  boost::asio::ip::tcp::endpoint listen_endpoint = boost::asio::ip::tcp::endpoint(local_address, listen_port);
  this->listen_socket = new boost::asio::ip::tcp::acceptor(ctx, listen_endpoint.protocol());
  try {
    this->listen_socket->bind(listen_endpoint);
  } catch (boost::system::system_error &e) {
    logger.write_error(e.what());
    exit(1);
  }
  logger.write_info("Server created.");
}

Server::~Server() {
  this->listen_socket->close();
  ctx.stop();
  this->thread_group->join_all();
  delete this->thread_group;
  logger.write_info("Closing proxy.");
}

void Server::listen() {
  try {
    this->listen_socket->listen();
  } catch (boost::system::system_error &e) {
    logger.write_error(e.what());
    exit(2);
  }
  logger.write_debug("Listening on port " + std::to_string(this->listen_socket->local_endpoint().port()));
  boost::asio::io_service::work work(ctx);
  for (int i = 0; i < THREAD_COUNT; i++) {
    this->thread_group->create_thread(boost::bind(&boost::asio::io_context::run, &ctx));
    logger.write_debug("Starting thread: " + std::to_string(i));
  }
  while (true) {
    try {
      boost::asio::ip::tcp::socket *client_socket = new boost::asio::ip::tcp::socket(ctx);
      this->listen_socket->accept(*client_socket);
      std::string client_address = client_socket->remote_endpoint().address().to_string();
      uint16_t client_port = client_socket->remote_endpoint().port();
      logger.write_info("Accepted connection from " + client_address + ":" + std::to_string(client_port) + ".");
      boost::asio::streambuf *stream_buffer = new boost::asio::streambuf();
      int bytes_transferred = boost::asio::read_until(*client_socket, *stream_buffer, END_OF_MESSAGE);
      std::string message = std::string(
        boost::asio::buffers_begin(stream_buffer->data()),
        boost::asio::buffers_begin(stream_buffer->data()) + bytes_transferred);
      stream_buffer->consume(bytes_transferred);
      std::string remaining = std::string(
        boost::asio::buffers_begin(stream_buffer->data()),
        boost::asio::buffers_begin(stream_buffer->data()) + stream_buffer->size());
      delete stream_buffer;
      try {
        auto connection = Connection::create(client_socket, message);
        connection->handle_connection(remaining);
      } catch (BadRequestException &e) {
        logger.write_warn(e.what());
      } catch (UnsupportedHTTPVersionException &e) {
        logger.write_warn(e.what());
      }
    } catch (boost::system::system_error &e) {
      logger.write_error(e.what());
      continue;
    }
  }
}
