#ifndef E18680A5_3B06_4019_A849_6CDB82D14796
#define E18680A5_3B06_4019_A849_6CDB82D14796
#include "src/server/user.hxx"
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <game_01_shared_class/serialization.hxx>
#include <string>
#include <vector>

boost::asio::awaitable<std::vector<std::string> > handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::map<size_t, User> &users, User &user);

boost::asio::awaitable<boost::optional<std::string> > createAccount (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool);

boost::asio::awaitable<boost::optional<std::string> > loginAccount (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, User &user);

void broadcastMessage (std::string const &msg, std::map<size_t, User> &users, User const &sendingUser);

boost::optional<std::string> joinChannel (std::string const &msg, User &user);

void leaveChannel (std::string const &msg, User &user);

#endif /* E18680A5_3B06_4019_A849_6CDB82D14796 */
