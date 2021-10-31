#ifndef HTTPS_PROXY_CONTEXT_HPP_
#define HTTPS_PROXY_CONTEXT_HPP_

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "logger/logger.hpp"
#include "blacklist.hpp"

struct context {
    boost::asio::io_context ctx;
    boost::asio::io_context accept_ctx;
    boost::asio::ip::tcp::resolver resolver;
    boost::mutex resolver_mutex;
    Logger logger;
    bool telemetry;
    Blacklist blacklist;
};

extern struct context ctx;

#endif  // HTTPS_PROXY_CONTEXT_HPP_
