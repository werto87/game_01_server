#ifndef E18680A5_3B06_4019_A849_6CDB82D14796
#define E18680A5_3B06_4019_A849_6CDB82D14796
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <durak/game.hxx>
#include <string>
#include <vector>

boost::asio::awaitable<std::vector<std::string> > handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::map<std::string, durak::Game> &games);

boost::asio::awaitable<boost::optional<std::string> > createAccount (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool);

boost::asio::awaitable<boost::optional<std::string> > loginAccount (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool);

boost::optional<std::string> createGame (std::string const &msg, std::map<std::string, durak::Game> &games);

#endif /* E18680A5_3B06_4019_A849_6CDB82D14796 */
