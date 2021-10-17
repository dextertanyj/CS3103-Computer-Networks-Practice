#ifndef HTTPS_PROXY_SERVER_HPP_
#define HTTPS_PROXY_SERVER_HPP_

#include <memory>

#include <boost/asio.hpp>
#include <boost/thread.hpp>

class Server : public std::enable_shared_from_this<Server> {
  public:
    static std::shared_ptr<Server> create(int);
    ~Server();
    void listen();
  private:
    explicit Server(int);
    std::unique_ptr<boost::thread_group> thread_group;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> listen_socket;

    void handle_accept(const boost::system::error_code&, boost::asio::ip::tcp::socket);
};

#endif  // HTTPS_PROXY_SERVER_HPP_
