#include "context.hpp"

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "logger.hpp"

#define LOG_FILE_PATH "./proxy.log"

context ctx = {
    .resolver = boost::asio::ip::tcp::resolver(ctx.ctx),
    .logger = Logger(LOG_FILE_PATH),
    .telemetry = false,
    .blacklist = Blacklist()
};
