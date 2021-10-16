#include "server.hpp"

#include <csignal>
#include <string>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include "connection.hpp"
#include "context.hpp"

#define ALL_INTERFACES {0, 0, 0, 0}
#define THREAD_COUNT 8
#define END_OF_MESSAGE "\r\n\r\n"

void interrupt_handler(int) {
  ctx.logger.write_info("Shutting down.");
  ctx.ctx.stop();
}

Server::Server(int port) : thread_group(std::make_unique<boost::thread_group>()) {
  uint16_t listen_port = boost::lexical_cast<uint16_t>(port);
  boost::asio::ip::address_v4 local_address = boost::asio::ip::address_v4(ALL_INTERFACES);
  boost::asio::ip::tcp::endpoint listen_endpoint = boost::asio::ip::tcp::endpoint(local_address, listen_port);
  this->listen_socket = std::make_shared<boost::asio::ip::tcp::acceptor>(ctx.ctx, listen_endpoint.protocol());
  try {
    this->listen_socket->bind(listen_endpoint);
  } catch (boost::system::system_error &e) {
    ctx.logger.write_fatal(e.what(), "Server::Server");
    exit(1);
  }
  ctx.logger.write_info("Server created.");
}

Server::~Server() {
  this->listen_socket->close();
}

std::shared_ptr<Server> Server::create(int port) {
  return std::shared_ptr<Server>(new Server(port));
}

void Server::listen() {
  ctx.ctx.reset();
  try {
    this->listen_socket->listen();
  } catch (boost::system::system_error &e) {
    ctx.logger.write_fatal(e.what(), "Server::listen");
    exit(2);
  }
  ctx.logger.write_info("Listening on port " + std::to_string(this->listen_socket->local_endpoint().port()));
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work(ctx.ctx.get_executor());
  for (int i = 0; i < THREAD_COUNT; i++) {
    this->thread_group->create_thread(boost::bind(&boost::asio::io_context::run, &(ctx.ctx)));
    ctx.logger.write_debug("Starting thread: " + std::to_string(i));
  }
  this->listen_socket->async_accept(ctx.ctx, std::bind(&Server::handle_accept, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
  std::signal(SIGINT, interrupt_handler);
  this->thread_group->join_all();
}

void Server::handle_accept(const boost::system::error_code &error, boost::asio::ip::tcp::socket peer_socket) {
  if (!error) {
    std::shared_ptr<boost::asio::ip::tcp::socket> client_socket = std::make_shared<boost::asio::ip::tcp::socket>(std::move(peer_socket));
    std::string client_address = client_socket->remote_endpoint().address().to_string();
    uint16_t client_port = client_socket->remote_endpoint().port();
    ctx.logger.write_debug("Accepted connection from " + client_address + ":" + std::to_string(client_port) + ".");
    boost::asio::streambuf *stream_buffer = new boost::asio::streambuf();
    int bytes_transferred = 0;
    try {
      bytes_transferred = boost::asio::read_until(*client_socket, *stream_buffer, END_OF_MESSAGE);
      std::string message = std::string(
        boost::asio::buffers_begin(stream_buffer->data()),
        boost::asio::buffers_begin(stream_buffer->data()) + bytes_transferred);
      stream_buffer->consume(bytes_transferred);
      std::string remaining = std::string(
        boost::asio::buffers_begin(stream_buffer->data()),
        boost::asio::buffers_begin(stream_buffer->data()) + stream_buffer->size());
      delete stream_buffer;
      ctx.logger.write_debug(message);
      std::shared_ptr<Connection> connection = Connection::create(client_socket, message);
      connection->handle_connection(remaining);
    } catch (boost::system::system_error &e) {
      ctx.logger.write_error(e.what(), "Server::handle_accept");
    } catch (BadRequestException &e) {
      ctx.logger.write_warn(e.what(), "Server::handle_accept");
    } catch (UnsupportedHTTPMethod &e) {
      ctx.logger.write_warn(e.what(), "Server::handle_accept");
    } catch (UnsupportedHTTPVersionException &e) {
      ctx.logger.write_warn(e.what(), "Server::handle_accept");
    } catch (BlockedException &e) {
      ctx.logger.write_info(e.what(), "Server::handle_accept");
    }
  } else {
    ctx.logger.write_error(error.message(), "Server::handle_accept");
  }
  this->listen_socket->async_accept(ctx.ctx, std::bind(&Server::handle_accept, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}
