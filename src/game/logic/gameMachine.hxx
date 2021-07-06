#ifndef CDC60584_086D_4538_8E80_2EE43DC1B49E
#define CDC60584_086D_4538_8E80_2EE43DC1B49E
#include "durak/game.hxx"
#include "src/game/logic/durakStateMachine.hxx"
#include <algorithm>
#include <iterator>
#include <ranges>

struct GameMachine
{
  explicit GameMachine (std::vector<std::shared_ptr<User>> users) : durakStateMachine{ my_logger{}, PassAttackAndAssist{}, _game, _users }, _users{ users }
  {
    auto names = std::vector<std::string>{};
    std::ranges::transform (_users, std::back_inserter (names), [] (auto const &tempUser) { return tempUser->accountName.value (); });
    _game = durak::Game{ std::move (names) };
  }

  GameMachine (durak::Game const &game, std::vector<std::shared_ptr<User>> &users) : durakStateMachine{ my_logger{}, PassAttackAndAssist{}, _game, _users }, _game{ game }, _users{ users } {}

  durak::Game const &
  getGame () const
  {
    return _game;
  }

  std::vector<std::shared_ptr<User>> const &
  getUsers () const
  {
    return _users;
  }

  DurakStateMachine durakStateMachine;

private:
  durak::Game _game{};
  std::vector<std::shared_ptr<User>> _users{};
};

#endif /* CDC60584_086D_4538_8E80_2EE43DC1B49E */
