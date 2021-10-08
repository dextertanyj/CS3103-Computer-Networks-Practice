#include "context.hpp"

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "logger.hpp"

boost::asio::io_context ctx;
boost::asio::ip::tcp::resolver resolver(ctx);

boost::mutex resolver_mutex;

Logger logger = Logger("./log");
