#ifndef CDC60584_086D_4538_8E80_2EE43DC1B49E
#define CDC60584_086D_4538_8E80_2EE43DC1B49E
#include "durak/game.hxx"
#include "src/game/gameUser.hxx"
#include "src/game/logic/durakStateMachine.hxx"
#include <algorithm>
#include <boost/asio/system_timer.hpp>
#include <iterator>
#include <range/v3/all.hpp>

struct GameMachine
{
  GameMachine (std::vector<std::shared_ptr<User>> users, boost::asio::io_context &io_context, TimerOption const &timerOption) : durakStateMachine{ my_logger{}, PassAttackAndAssist{}, _game, _gameUsers, timerOption }
  {
    ranges::transform (users, ranges::back_inserter (_gameUsers), [&io_context] (auto const &user) { return GameUser{ user, std::make_shared<boost::asio::system_timer> (io_context) }; });
    auto names = std::vector<std::string>{};
    ranges::transform (_gameUsers, ranges::back_inserter (names), [] (auto const &gameUser) { return gameUser._user->accountName.value (); });
    _game = durak::Game{ std::move (names) };
    durakStateMachine.process_event (start{});
    durakStateMachine.process_event (initTimer{});
  }

  GameMachine (durak::Game const &game, std::vector<std::shared_ptr<User>> &users, boost::asio::io_context &io_context, TimerOption const &timerOption) : durakStateMachine{ my_logger{}, PassAttackAndAssist{}, _game, _gameUsers, timerOption }, _game{ game }
  {
    ranges::transform (users, ranges::back_inserter (_gameUsers), [&io_context] (auto const &user) { return GameUser{ user, std::make_shared<boost::asio::system_timer> (io_context) }; });
    durakStateMachine.process_event (start{});
    durakStateMachine.process_event (initTimer{});
  }

  durak::Game const &
  getGame () const
  {
    return _game;
  }

  void
  relogUser (std::shared_ptr<User> &user)
  {
    if (auto oldLogin = ranges::find_if (_gameUsers, [accountName = user->accountName.value ()] (auto const &gameUser) { return accountName == gameUser._user->accountName.value (); }); oldLogin != _gameUsers.end ())
      {
        oldLogin->_user = user;
      }
    else
      {
        throw std::logic_error{ "can not relog user beacuse he is not logged in the game" };
      }
  }

  std::vector<GameUser> const &
  getGameUsers () const
  {
    return _gameUsers;
  }

  DurakStateMachine durakStateMachine;

private:
  durak::Game _game{};
  std::vector<GameUser> _gameUsers;
};

#endif /* CDC60584_086D_4538_8E80_2EE43DC1B49E */
