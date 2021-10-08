#include "context.hpp"

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "logger.hpp"

context ctx = {
    .resolver = boost::asio::ip::tcp::resolver(ctx.ctx),
    .logger = Logger("./log"),
    .telemetry = false,
    .blacklist = Blacklist()
};
