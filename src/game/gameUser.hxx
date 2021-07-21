#ifndef CD87DBC4_A34E_4656_9B63_4310D079E794
#define CD87DBC4_A34E_4656_9B63_4310D079E794
#include "src/server/user.hxx"
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/system_timer.hpp>
#include <chrono>
#include <iostream>
#include <memory>

struct GameUser
{
  GameUser (std::shared_ptr<User> user, std::shared_ptr<boost::asio::system_timer> timer) : _user{ user }, _timer{ timer } {}

  std::shared_ptr<User> _user{};
  std::shared_ptr<boost::asio::system_timer> _timer;
  std::optional<std::chrono::milliseconds> _pausedTime{};
};

#endif /* CD87DBC4_A34E_4656_9B63_4310D079E794 */
