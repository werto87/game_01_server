#ifndef EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4
#define EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4
#include "durak/game.hxx"
#include "durak/gameData.hxx"
#include "src/game/gameUser.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include "src/server/user.hxx"
#include "src/util.hxx"
#include <boost/asio/co_spawn.hpp>
#include <boost/sml.hpp>
#include <fmt/format.h>
#include <game_01_shared_class/serialization.hxx>
#include <iostream>
#include <optional>
#include <queue>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/all.hpp>
#include <vector>

struct my_logger
{
  template <class SM, class TEvent>
  void
  log_process_event (const TEvent &)
  {
    // if(std::string{sml::aux::get_type_name<TEvent> ()}!=std::string{"draw"}){
    //   printf ("[%s][process_event] %s\n", sml::aux::get_type_name<SM> (), sml::aux::get_type_name<TEvent> ());
    // }
  }

  template <class SM, class TGuard, class TEvent>
  void
  log_guard (const TGuard &, const TEvent &, bool /*result*/)
  {
    // printf ("[%s][guard] %s %s %s\n", sml::aux::get_type_name<SM> (), sml::aux::get_type_name<TGuard> (), sml::aux::get_type_name<TEvent> (), (result ? "[OK]" : "[Reject]"));
  }

  template <class SM, class TAction, class TEvent>
  void
  log_action (const TAction &, const TEvent &)
  {
    // printf ("[%s][action] %s %s\n", sml::aux::get_type_name<SM> (), sml::aux::get_type_name<TAction> (), sml::aux::get_type_name<TEvent> ());
  }

  template <class SM, class TSrcState, class TDstState>
  void
  log_state_change (const TSrcState &src, const TDstState &dst)
  {
    printf ("[%s] %s -> %s\n", boost::sml::aux::get_type_name<SM> (), src.c_str (), dst.c_str ());
  }
};

auto const timerActive = [] (TimerOption &timerOption) { return timerOption.timerType != TimerType::noTimer; };

inline void
removeUserFromGame (std::string const &userToRemove, durak::Game &game, std::vector<GameUser> &_gameUsers)
{
  if (not game.checkIfGameIsOver ())
    {
      game.removePlayer (userToRemove);

      if (auto durak = game.durak ())
        {
          ranges::for_each (_gameUsers, [durak = durak->id] (auto const &gameUser) {
            if (gameUser._user->accountName == durak) gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverLose{}));
            else
              gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverWon{}));
          });
        }
      else
        {
          ranges::for_each (_gameUsers, [] (auto const &gameUser) { gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverDraw{})); });
        }
    }
}

boost::asio::awaitable<void> inline runTimer (std::shared_ptr<boost::asio::steady_timer> timer, std::string const &accountName, durak::Game &game, std::vector<GameUser> &_gameUsers)
{
  try
    {
      co_await timer->async_wait (boost::asio::use_awaitable);
      removeUserFromGame (accountName, game, _gameUsers);
      ranges::for_each (_gameUsers, [] (auto const &gameUser) { gameUser._timer->cancel (); });
    }
  catch (boost::system::system_error &e)
    {
      using namespace boost::system::errc;
      if (operation_canceled == e.code ())
        {
          // swallow cancel
        }
      else
        {
          std::cout << "error in timer boost::system::errc: " << e.code () << std::endl;
          abort ();
        }
    }
}

auto const initTimerHandler = [] (durak::Game &game, std::vector<GameUser> &_gameUsers, TimerOption &timerOption) {
  ranges::for_each (_gameUsers, [&timerOption, &game, &_gameUsers] (auto &gameUser) {
    gameUser._timer->expires_after (timerOption.timeAtStart);
    co_spawn (
        gameUser._timer->get_executor (), [_timer = gameUser._timer, accountName = gameUser._user->accountName.value (), &game, &_gameUsers] () { return runTimer (_timer, accountName, game, _gameUsers); }, boost::asio::detached);
  });
};

auto const pauseTimerHandler = [] (std::vector<GameUser> &_gameUsers) {
  ranges::for_each (_gameUsers, [] (auto &gameUser) {
    using namespace std::chrono;
    gameUser._pausedTime = duration_cast<milliseconds> (gameUser._timer->expiry () - steady_clock::now ());
    gameUser._timer->cancel ();
  });
};

auto const nextRoundTimerHandler = [] (durak::Game &game, std::vector<GameUser> &_gameUsers, TimerOption &timerOption) {
  ranges::for_each (_gameUsers, [&timerOption, &game, &_gameUsers] (auto &gameUser) {
    using namespace std::chrono;
    if (timerOption.timerType == TimerType::addTimeOnNewRound)
      {
        gameUser._timer->expires_after (timerOption.timeForEachRound + duration_cast<milliseconds> (gameUser._timer->expiry () - steady_clock::now ()));
      }
    else
      {
        gameUser._timer->expires_after (timerOption.timeForEachRound);
      }
    co_spawn (
        gameUser._timer->get_executor (), [_timer = gameUser._timer, accountName = gameUser._user->accountName.value (), &game, &_gameUsers] () { return runTimer (_timer, accountName, game, _gameUsers); }, boost::asio::detached);
  });
};

auto const resumeTimerHandler = [] (durak::Game &game, std::vector<GameUser> &_gameUsers) {
  ranges::for_each (_gameUsers, [&game, &_gameUsers] (auto &gameUser) {
    gameUser._timer->expires_after (gameUser._pausedTime);
    co_spawn (
        gameUser._timer->get_executor (), [_timer = gameUser._timer, accountName = gameUser._user->accountName.value (), &game, &_gameUsers] () { return runTimer (_timer, accountName, game, _gameUsers); }, boost::asio::detached);
  });
};

// care const& is not &. AttackAndAssistAnswer &attackAndAssistAnswer is another object than AttackAndAssistAnswer const&attackAndAssistAnswer

auto const allCardsBeaten = [] (durak::Game &game) {
  //
  return game.countOfNotBeatenCardsOnTable () == 0 && game.getAttackStarted ();
};

auto const isDefendingPlayer = [] (defend const &defendEv, durak::Game &game) { return game.getRoleForName (defendEv.playerName) == durak::PlayerRole::defend; };

auto const isDefendingPlayerAndHasCardsOnTable = [] (defend const &defendEv, durak::Game &game) { return game.getRoleForName (defendEv.playerName) == durak::PlayerRole::defend && game.getAttackStarted (); };

auto const setAttackAnswer = [] (attackPass const &attackPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = attackPassEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto &sendingUserMsgQueue = gameUserItr->_user->msgQueue;
      if (not passAttackAndAssist.attack)
        {
          if (game.getRoleForName (attackPassEv.playerName) == durak::PlayerRole::attack)
            {
              passAttackAndAssist.attack = true;
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsSuccess{}));
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ "role is not attack" }));
            }
        }
      else
        {
          sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ "pass already set" }));
        }
    }
};
auto const setAssistAnswer = [] (assistPass const &assistPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = assistPassEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto &sendingUserMsgQueue = gameUserItr->_user->msgQueue;
      if (not passAttackAndAssist.assist)
        {
          if (game.getRoleForName (assistPassEv.playerName) == durak::PlayerRole::assistAttacker)
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsSuccess{}));
              passAttackAndAssist.assist = true;
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ "role is not assist" }));
            }
        }
      else
        {
          sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ "pass already set" }));
        }
    }
};

auto const checkData = [] (PassAttackAndAssist &passAttackAndAssist, durak::Game &game, boost::sml::back::process<askDef> process_event) {
  if (auto attackingPlayer = game.getAttackingPlayer (); not attackingPlayer || attackingPlayer->getCards ().empty ())
    {
      passAttackAndAssist.attack = true;
    }
  if (auto assistingPlayer = game.getAssistingPlayer (); not assistingPlayer || assistingPlayer->getCards ().empty ())
    {
      passAttackAndAssist.assist = true;
    }
  if (passAttackAndAssist.assist && passAttackAndAssist.attack && game.countOfNotBeatenCardsOnTable () == 0)
    {
      process_event (askDef{});
    }
};

auto const checkAttackAndAssistAnswer = [] (PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<chill, nextRoundTimer> process_event) {
  if (auto attackingPlayer = game.getAttackingPlayer (); not attackingPlayer || attackingPlayer->getCards ().empty ())
    {
      passAttackAndAssist.attack = true;
    }
  if (auto assistingPlayer = game.getAssistingPlayer (); not assistingPlayer || assistingPlayer->getCards ().empty ())
    {
      passAttackAndAssist.assist = true;
    }
  if (passAttackAndAssist.attack && passAttackAndAssist.assist)
    {
      game.nextRound (true);
      process_event (nextRoundTimer{});
      sendGameDataToAccountsInGame (game, _gameUsers);
      if (game.checkIfGameIsOver ())
        {
          if (auto durak = game.durak ())
            {
              ranges::for_each (_gameUsers, [durak = durak->id] (auto const &gameUser) {
                if (gameUser._user->accountName == durak) gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverLose{}));
                else
                  gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverWon{}));
              });
            }
          else
            {
              ranges::for_each (_gameUsers, [] (auto const &gameUser) { gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverDraw{})); });
            }
        }
      process_event (chill{});
    }
};

auto const startAskDef = [] (durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto defendingPlayer = game.getDefendingPlayer ())
    {
      if (auto gameUserItr = ranges::find_if (_gameUsers, [&defendingPlayer] (auto const &gameUser) { return gameUser._user->accountName.value () == defendingPlayer->id; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCards{}));
        }
    }
};

auto const userLeftGame = [] (leaveGame const &leaveGameEv, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  removeUserFromGame (leaveGameEv.accountName, game, _gameUsers);
  ranges::for_each (_gameUsers, [] (auto const &gameUser) { gameUser._timer->cancel (); });
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = leaveGameEv.accountName] (auto const &gameUser) { return gameUser._user->accountName == accountName; }); gameUserItr != _gameUsers.end ())
    {
      _gameUsers.erase (gameUserItr);
    }
};

auto const startAskAttackAndAssist = [] (PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<nextRoundTimer> process_event) {
  passAttackAndAssist = PassAttackAndAssist{};
  if (auto attackingPlayer = game.getAttackingPlayer (); attackingPlayer && not attackingPlayer->getCards ().empty ())
    {
      if (auto gameUserItr = ranges::find_if (_gameUsers, [&attackingPlayer] (auto const &gameUser) { return gameUser._user->accountName.value () == attackingPlayer->id; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards{}));
        }
    }
  else
    {
      passAttackAndAssist.attack = true;
    }
  if (auto assistingPlayer = game.getAssistingPlayer (); assistingPlayer && not assistingPlayer->getCards ().empty ())
    {
      if (auto gameUserItr = ranges::find_if (_gameUsers, [&assistingPlayer] (auto const &gameUser) { return gameUser._user->accountName.value () == assistingPlayer->id; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards{}));
        }
    }
  else
    {
      passAttackAndAssist.assist = true;
    }
  if (passAttackAndAssist.assist && passAttackAndAssist.attack)
    {
      game.nextRound (true);
      process_event (nextRoundTimer{});
      sendGameDataToAccountsInGame (game, _gameUsers);
      if (game.checkIfGameIsOver ())
        {
          if (auto durak = game.durak ())
            {
              ranges::for_each (_gameUsers, [durak = durak->id] (auto const &gameUser) {
                if (gameUser._user->accountName == durak) gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverLose{}));
                else
                  gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverWon{}));
              });
            }
          else
            {
              ranges::for_each (_gameUsers, [] (auto const &gameUser) { gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverDraw{})); });
            }
        }
    }
};

auto const setAttackPass = [] (attackPass const &attackPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = attackPassEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto &sendingUserMsgQueue = gameUserItr->_user->msgQueue;
      auto playerRole = game.getRoleForName (attackPassEv.playerName);
      if (game.getAttackStarted ())
        {
          if (game.countOfNotBeatenCardsOnTable () == 0)
            {
              if (playerRole == durak::PlayerRole::attack)
                {
                  passAttackAndAssist.attack = true;
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackPassSuccess{}));
                }
              else
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackPassError{ "account role is not attack: " + attackPassEv.playerName }));
                }
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackPassError{ "there are not beaten cards on the table" }));
            }
        }
      else
        {
          sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackPassError{ "can not pass if attack is not started" }));
        }
    }
};

auto const setAssistPass = [] (assistPass const &assistPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = assistPassEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto &sendingUserMsgQueue = gameUserItr->_user->msgQueue;
      auto playerRole = game.getRoleForName (assistPassEv.playerName);
      if (game.getAttackStarted ())
        {
          if (game.countOfNotBeatenCardsOnTable () == 0)
            {
              if (playerRole == durak::PlayerRole::assistAttacker)
                {
                  passAttackAndAssist.assist = true;
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAssistPassSuccess{}));
                }
              else
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAssistPassError{ "account role is not assist: " + assistPassEv.playerName }));
                }
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAssistPassError{ "there are not beaten cards on the table" }));
            }
        }
      else
        {
          sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAssistPassError{ "can not pass if attack is not started" }));
        }
    }
};

auto const handleDefendSuccess = [] (defendAnswerNo const &defendAnswerNoEv, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<nextRoundTimer> process_event) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = defendAnswerNoEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto &sendingUserMsgQueue = gameUserItr->_user->msgQueue;
      if (game.getRoleForName (defendAnswerNoEv.playerName) == durak::PlayerRole::defend)
        {
          game.nextRound (false);
          process_event (nextRoundTimer{});
          if (game.checkIfGameIsOver ())
            {
              if (auto durak = game.durak ())
                {
                  ranges::for_each (_gameUsers, [durak = durak->id] (auto const &gameUser) {
                    if (gameUser._user->accountName == durak) gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverLose{}));
                    else
                      gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverWon{}));
                  });
                }
              else
                {
                  ranges::for_each (_gameUsers, [] (auto const &gameUser) { gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverDraw{})); });
                }
            }
          sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCardsAnswerSuccess{}));
          sendGameDataToAccountsInGame (game, _gameUsers);
        }
      else
        {
          sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCardsAnswerError{ "account role is not defend: " + defendAnswerNoEv.playerName }));
        }
    }
};

auto const handleDefendPass = [] (defendPass const &defendPassEv, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<askAttackAndAssist> process_event) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = defendPassEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto &sendingUserMsgQueue = gameUserItr->_user->msgQueue;
      if (game.getRoleForName (defendPassEv.playerName) == durak::PlayerRole::defend)
        {
          if (game.getAttackStarted ())
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendPassSuccess{}));
              sendGameDataToAccountsInGame (game, _gameUsers);
              process_event (askAttackAndAssist{});
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendPassError{ "attack is not started" }));
            }
        }
      else
        {
          sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendPassError{ "account role is not defiend: " + defendPassEv.playerName }));
        }
    }
};

auto const resetPassStateMachineData = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist = PassAttackAndAssist{}; };

auto const resetPassAttackAndAssist = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist = PassAttackAndAssist{}; };

auto const setRewokePassAssist = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist.assist = false; };
auto const setRewokePassAttack = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist.attack = false; };

auto const doAttack = [] (PassAttackAndAssist &passAttackAndAssist, attack const &attackEv, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = attackEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto &sendingUserMsgQueue = gameUserItr->_user->msgQueue;
      auto playerRole = game.getRoleForName (attackEv.playerName);
      if (playerRole == durak::PlayerRole::attack)
        {
          if (game.getAttackStarted ())
            {
              if (game.playerAssists (playerRole, attackEv.cards))
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackSuccess{}));
                  sendGameDataToAccountsInGame (game, _gameUsers);
                  passAttackAndAssist = PassAttackAndAssist{};
                }
              else
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackError{ "not allowed to play cards" }));
                }
            }
          else
            {
              if (game.playerStartsAttack (attackEv.cards))
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackSuccess{}));
                  sendGameDataToAccountsInGame (game, _gameUsers);
                  passAttackAndAssist = PassAttackAndAssist{};
                }
              else
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackError{ "not allowed to play cards" }));
                }
            }
        }
      else if (playerRole == durak::PlayerRole::assistAttacker)
        {
          if (game.getAttackStarted ())
            {
              if (game.playerAssists (playerRole, attackEv.cards))
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackSuccess{}));
                  sendGameDataToAccountsInGame (game, _gameUsers);
                  passAttackAndAssist = PassAttackAndAssist{};
                }
              else
                {
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackError{ "not allowed to play cards" }));
                }
            }
        }
    }
};

auto const doDefend = [] (defend const &defendEv, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = defendEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto &sendingUserMsgQueue = gameUserItr->_user->msgQueue;
      auto playerRole = game.getRoleForName (defendEv.playerName);
      if (playerRole == durak::PlayerRole::defend)
        {
          if (game.playerDefends (defendEv.cardToBeat, defendEv.card))
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendSuccess{}));
              sendGameDataToAccountsInGame (game, _gameUsers);
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendError{ "Error while defending " + fmt::format ("CardToBeat: {},{} vs. Card: {},{}", defendEv.cardToBeat.value, magic_enum::enum_name (defendEv.cardToBeat.type), defendEv.card.value, magic_enum::enum_name (defendEv.card.type)) }));
            }
        }
    }
};

auto const askAttackAgain = [] (PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<nextRoundTimer> process_event) {
  if (auto attackingPlayer = game.getAttackingPlayer (); not passAttackAndAssist.attack && attackingPlayer && not attackingPlayer->getCards ().empty ())
    {
      if (auto gameUserItr = ranges::find_if (_gameUsers, [&attackingPlayer] (auto const &gameUser) { return gameUser._user->accountName.value () == attackingPlayer->id; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards{}));
        }
    }
  else
    {
      passAttackAndAssist.attack = true;
    }
  if (auto assistingPlayer = game.getAssistingPlayer (); not(assistingPlayer && not assistingPlayer->getCards ().empty ()))
    {
      passAttackAndAssist.assist = true;
    }
  if (passAttackAndAssist.assist && passAttackAndAssist.attack)
    {

      game.nextRound (true);
      process_event (nextRoundTimer{});
      // if (timerActive (timerOption))
      //   {
      //     nextRoundTimerHandler (game, _gameUsers, timerOption);
      //   }
      sendGameDataToAccountsInGame (game, _gameUsers);
      if (game.checkIfGameIsOver ())
        {
          if (auto durak = game.durak ())
            {
              ranges::for_each (_gameUsers, [durak = durak->id] (auto const &gameUser) {
                if (gameUser._user->accountName == durak) gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverLose{}));
                else
                  gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverWon{}));
              });
            }
          else
            {
              ranges::for_each (_gameUsers, [] (auto const &gameUser) { gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverDraw{})); });
            }
        }
    }
};
auto const askAssistAgain = [] (PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<nextRoundTimer> process_event) {
  if (auto attackingPlayer = game.getAttackingPlayer (); not(attackingPlayer && not attackingPlayer->getCards ().empty ()))
    {
      passAttackAndAssist.attack = true;
    }
  if (auto assistingPlayer = game.getAssistingPlayer (); assistingPlayer && not assistingPlayer->getCards ().empty ())
    {
      if (auto gameUserItr = ranges::find_if (_gameUsers, [&assistingPlayer] (auto const &gameUser) { return gameUser._user->accountName.value () == assistingPlayer->id; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards{}));
        }
    }
  else
    {
      passAttackAndAssist.assist = true;
    }
  if (passAttackAndAssist.assist && passAttackAndAssist.attack)
    {
      game.nextRound (true);
      process_event (nextRoundTimer{});
      // if (timerActive (timerOption))
      //   {
      //     nextRoundTimerHandler (game, _gameUsers, timerOption);
      //   }
      sendGameDataToAccountsInGame (game, _gameUsers);
      if (game.checkIfGameIsOver ())
        {
          if (auto durak = game.durak ())
            {
              ranges::for_each (_gameUsers, [durak = durak->id] (auto const &gameUser) {
                if (gameUser._user->accountName == durak) gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverLose{}));
                else
                  gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverWon{}));
              });
            }
          else
            {
              ranges::for_each (_gameUsers, [] (auto const &gameUser) { gameUser._user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakGameOverDraw{})); });
            }
        }
    }
};

struct PassMachine
{
  auto
  operator() () const
  {
    using namespace boost::sml;
    return make_transition_table (
        // clang-format off
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
* state<Chill>                  + on_entry<_>                                             /(resetPassStateMachineData)           
, state<Chill>                  + event<askDef>                                                                                       = state<AskDef>
, state<Chill>                  + event<askAttackAndAssist>                                                                           = state<AskAttackAndAssist>
, state<Chill>                  + event<attackPass>                                       /(setAttackPass,checkData)
, state<Chill>                  + event<assistPass>                                       /(setAssistPass,checkData)
, state<Chill>                  + event<defendPass>                                       / handleDefendPass                                         
, state<Chill>                  + event<attack>                                           / doAttack 
, state<Chill>                  + event<defend>                 [isDefendingPlayer]       / doDefend 
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
, state<AskDef>                 + on_entry<_>                                             / startAskDef
, state<AskDef>                 + event<defendAnswerYes>                                                                                = state<AskAttackAndAssist>
, state<AskDef>                 + event<defendAnswerNo>                                   / handleDefendSuccess                         = state<Chill>
, state<AskDef>                 + event<defendRelog>                                      / startAskDef
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
, state<AskAttackAndAssist>     + on_entry<_>                                             / startAskAttackAndAssist
, state<AskAttackAndAssist>     + event<attackPass>                                       /(setAttackAnswer,checkAttackAndAssistAnswer)
, state<AskAttackAndAssist>     + event<assistPass>                                       /(setAssistAnswer,checkAttackAndAssistAnswer)
, state<AskAttackAndAssist>     + event<attack>                                           / doAttack 
, state<AskAttackAndAssist>     + event<chill>                                                                                          =state<Chill>
, state<AskAttackAndAssist>     + event<attackRelog>                                      / askAttackAgain
, state<AskAttackAndAssist>     + event<assistRelog>                                      / askAssistAgain
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
,*"leaveGameHandler"_s          + event<leaveGame>                                        / userLeftGame                                
,*"timerHandler"_s              + event<initTimer>              [timerActive]             / initTimerHandler                               
, "timerHandler"_s              + event<nextRoundTimer>         [timerActive]             / nextRoundTimerHandler
, "timerHandler"_s              + event<pauseTimer>             [timerActive]             / pauseTimerHandler
, "timerHandler"_s              + event<resumeTimer>            [timerActive]             / resumeTimerHandler
// clang-format on   
    );
  }
};

typedef boost::sml::sm<PassMachine, boost::sml::logger<my_logger>,boost::sml::process_queue<std::queue>> DurakStateMachine;



#endif /* EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4 */
