#ifndef F85705C8_6F01_4F50_98CA_5636F5F5E1C1
#define F85705C8_6F01_4F50_98CA_5636F5F5E1C1

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/optional.hpp>
#include <cstddef>
#include <deque>
#include <list>
#include <memory>
#include <queue>
#include <set>
#include <string>

typedef boost::asio::use_awaitable_t<>::as_default_on_t<boost::asio::basic_waitable_timer<boost::asio::chrono::system_clock>> CoroTimer;
typedef boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> SSLWebsocket;

struct User
{
  boost::asio::awaitable<void> writeToClient (std::weak_ptr<SSLWebsocket> &connection);
  void sendMessageToUser (std::string const &message);

  boost::optional<std::string> accountName{}; // has value if user is logged in
  std::set<std::string> communicationChannels{};
  bool ignoreLogin{};
  bool ignoreCreateAccount{};
  std::deque<std::string> msgQueue{};
  std::shared_ptr<CoroTimer> timer{};
  bool isTimerRunning = false;
};

#endif /* F85705C8_6F01_4F50_98CA_5636F5F5E1C1 */
