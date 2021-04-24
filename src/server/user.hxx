#ifndef F85705C8_6F01_4F50_98CA_5636F5F5E1C1
#define F85705C8_6F01_4F50_98CA_5636F5F5E1C1

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/optional.hpp>
#include <deque>
#include <set>
#include <string>

struct User
{
  User () = default;
  User (boost::optional<std::string> _accountId, boost::beast::websocket::stream<boost::beast::tcp_stream> &&_websocket, std::deque<std::string> _msgQueue, std::set<std::string> _communicationChannels) : accountId{ std::move (_accountId) }, websocket{ std::move (_websocket) }, msgQueue{ std::move (_msgQueue) }, communicationChannels{ std::move (_communicationChannels) } {};
  User (const User &) = delete;
  const User &operator= (const User &) = delete;
  User (User &&) = default;
  User &operator= (User &&) = delete;
  //~User () { websocket.close ("user goes offline"); }

  boost::optional<std::string> accountId{}; // has value if user is logged in
  boost::beast::websocket::stream<boost::beast::tcp_stream> websocket;
  std::deque<std::string> msgQueue{};
  std::set<std::string> communicationChannels{};
};

#endif /* F85705C8_6F01_4F50_98CA_5636F5F5E1C1 */
