#ifndef HTTPS_PROXY_SERVER_HPP_
#define HTTPS_PROXY_SERVER_HPP_

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "logger.hpp"

class Server {
  private:
    boost::thread_group *thread_group;
    boost::asio::ip::tcp::acceptor *listen_socket;
  public:
    explicit Server(int);
    ~Server();
    void listen();
};

#endif  // HTTPS_PROXY_SERVER_HPP_
