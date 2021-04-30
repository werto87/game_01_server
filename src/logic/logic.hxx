#ifndef E18680A5_3B06_4019_A849_6CDB82D14796
#define E18680A5_3B06_4019_A849_6CDB82D14796
#include "src/server/user.hxx"
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <game_01_shared_class/serialization.hxx>
#include <string>
#include <vector>

boost::asio::awaitable<std::vector<std::string> > handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User> > &users, User &user);

boost::asio::awaitable<std::string> createAccount (std::string objectAsString, boost::asio::io_context &io_context, boost::asio::thread_pool &pool);

boost::asio::awaitable<std::string> loginAccount (std::string objectAsString, boost::asio::io_context &io_context, std::list<std::shared_ptr<User> > &users, User &user, boost::asio::thread_pool &pool);

std::string logoutAccount (User &user);

std::string broadCastMessage (std::string const &objectAsString, std::list<std::shared_ptr<User> > &users, User const &sendingUser);

std::string joinChannel (std::string const &objectAsString, User &user);

void leaveChannel (std::string const &objectAsString, User &user);

std::string createGameLobby (User &user);

std::string joinGameLobby (User &user);

template <typename TypeToSend>
std::string
objectToStringWithObjectName (TypeToSend const &typeToSend)
{
  return confu_soci::typeNameWithOutNamespace (typeToSend) + '|' + confu_boost::toString (typeToSend);
}

#endif /* E18680A5_3B06_4019_A849_6CDB82D14796 */
