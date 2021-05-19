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
  boost::optional<std::string> accountName{}; // has value if user is logged in
  std::deque<std::string> msgQueue{};
  std::set<std::string> communicationChannels{};
  bool ignoreLogin{};
  bool ignoreCreateAccount{};
};

#endif /* F85705C8_6F01_4F50_98CA_5636F5F5E1C1 */
