#ifndef AD140436_3FBA_4D63_8C0E_9113B92859E0
#define AD140436_3FBA_4D63_8C0E_9113B92859E0

#include "src/database/database.hxx"
#include "src/server/game.hxx"
#include "src/server/gameLobby.hxx"
#include "src/server/user.hxx"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <cstddef>
#include <list>
#include <memory>
#include <queue>
#include <string>
class Server
{
public:
  Server (boost::asio::io_context &io_context, boost::asio::thread_pool &pool);

  boost::asio::awaitable<void> listener ();

private:
  void removeUser (std::list<std::shared_ptr<User>>::iterator user);
  boost::asio::awaitable<std::string> my_read (boost::beast::websocket::stream<boost::beast::tcp_stream> &ws_);

  boost::asio::awaitable<void> readFromClient (std::list<std::shared_ptr<User>>::iterator user);

  boost::asio::awaitable<void> writeToClient (std::shared_ptr<User> user);

  boost::asio::io_context &_io_context;
  boost::asio::thread_pool &_pool;
  std::list<std::shared_ptr<User>> users{};
  std::list<GameLobby> gameLobbys{};
  std::list<Game> games{};
};

#endif /* AD140436_3FBA_4D63_8C0E_9113B92859E0 */
