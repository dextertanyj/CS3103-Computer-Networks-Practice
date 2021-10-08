#ifndef HTTPS_PROXY_CONTEXT_HPP_
#define HTTPS_PROXY_CONTEXT_HPP_

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "logger.hpp"

extern boost::asio::io_context ctx;
extern boost::asio::ip::tcp::resolver resolver;

extern boost::mutex resolver_mutex;

extern Logger logger;

#endif  // HTTPS_PROXY_CONTEXT_HPP_
