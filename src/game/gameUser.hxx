#ifndef CD87DBC4_A34E_4656_9B63_4310D079E794
#define CD87DBC4_A34E_4656_9B63_4310D079E794
#include "src/server/user.hxx"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>

struct GameUser
{
  GameUser (std::shared_ptr<User> user, std::shared_ptr<boost::asio::steady_timer> timer) : _user{ user }, _timer{ timer } {}
  std::shared_ptr<User> _user{};
  std::shared_ptr<boost::asio::steady_timer> _timer;
};

#endif /* CD87DBC4_A34E_4656_9B63_4310D079E794 */
