#ifndef EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4
#define EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4
#include "durak/game.hxx"
#include "durak/gameData.hxx"
#include "src/game/gameUser.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include "src/server/user.hxx"
#include "src/util.hxx"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/sml.hpp>
#include <chrono>
#include <cstdlib>
#include <fmt/format.h>
#include <game_01_shared_class/serialization.hxx>
#include <iostream>
#include <optional>
#include <queue>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/all.hpp>
#include <utility>
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

auto const timerActive = [] (TimerOption &timerOption) { return timerOption.timerType != shared_class::TimerType::noTimer; };

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

boost::asio::awaitable<void> inline runTimer (std::shared_ptr<boost::asio::system_timer> timer, std::string const &accountName, durak::Game &game, std::vector<GameUser> &_gameUsers, std::function<void ()> gameOverCallback)
{
  try
    {
      co_await timer->async_wait (boost::asio::use_awaitable);
      removeUserFromGame (accountName, game, _gameUsers);
      ranges::for_each (_gameUsers, [] (auto const &gameUser) { gameUser._timer->cancel (); });
      gameOverCallback ();
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

auto const sendTimer = [] (std::vector<GameUser> &_gameUsers, TimerOption &timerOption) {
  if (timerOption.timerType != shared_class::TimerType::noTimer)
    {
      auto durakTimers = shared_class::DurakTimers{};
      ranges::for_each (_gameUsers, [&durakTimers] (GameUser const &gameUser) {
        if (gameUser._pausedTime)
          {
            durakTimers.pausedTimeUserDurationMilliseconds.push_back (std::make_pair (gameUser._user->accountName.value (), gameUser._pausedTime->count ()));
          }
        else
          {
            using namespace std::chrono;
            durakTimers.runningTimeUserTimePointMilliseconds.push_back (std::make_pair (gameUser._user->accountName.value (), duration_cast<milliseconds> (gameUser._timer->expiry ().time_since_epoch ()).count ()));
          }
        gameUser._user->msgQueue.push_back (objectToStringWithObjectName (durakTimers));
      });
      ranges::for_each (_gameUsers, [&durakTimers] (auto const &gameUser) { gameUser._user->msgQueue.push_back (objectToStringWithObjectName (durakTimers)); });
    }
};

auto const initTimerHandler = [] (durak::Game &game, std::vector<GameUser> &_gameUsers, TimerOption &timerOption, boost::sml::back::process<resumeTimer> process_event) {
  using namespace std::chrono;
  ranges::for_each (_gameUsers, [&timerOption] (auto &gameUser) { gameUser._pausedTime = timerOption.timeAtStart; });
  if (auto attackingPlayer = game.getAttackingPlayer ())
    {
      process_event (resumeTimer{ { attackingPlayer->id } });
    }
};

auto const pauseTimerHandler = [] (pauseTimer const &pauseTimerEv, std::vector<GameUser> &_gameUsers) {
  ranges::for_each (_gameUsers, [&playersToPausetime = pauseTimerEv.playersToPause] (auto &gameUser) {
    using namespace std::chrono;
    if (ranges::find (playersToPausetime, gameUser._user->accountName.value ()) != playersToPausetime.end ())
      {
        gameUser._pausedTime = duration_cast<milliseconds> (gameUser._timer->expiry () - system_clock::now ());
        gameUser._timer->cancel ();
      }
  });
};

auto const nextRoundTimerHandler = [] (durak::Game &game, std::vector<GameUser> &_gameUsers, TimerOption &timerOption, boost::sml::back::process<resumeTimer> process_event) {
  using namespace std::chrono;
  ranges::for_each (_gameUsers, [&timerOption] (auto &gameUser) {
    if (timerOption.timerType == shared_class::TimerType::addTimeOnNewRound)
      {
        gameUser._pausedTime = timerOption.timeForEachRound + duration_cast<milliseconds> (gameUser._timer->expiry () - system_clock::now ());
      }
    else
      {
        gameUser._pausedTime = timerOption.timeForEachRound;
      }
    gameUser._timer->cancel ();
  });
  if (auto attackingPlayer = game.getAttackingPlayer ())
    {
      process_event (resumeTimer{ { attackingPlayer->id } });
    }
};
auto const blockDef = AllowedMoves{ .defend = std::vector<shared_class::Move>{} };
auto const blockAttackAndAssist = AllowedMoves{ .attack = std::vector<shared_class::Move>{}, .assist = std::vector<shared_class::Move>{} };

auto const blockEverythingExceptStartAttack = AllowedMoves{ .defend = std::vector<shared_class::Move>{}, .attack = std::vector<shared_class::Move>{ shared_class::Move::AddCards }, .assist = std::vector<shared_class::Move>{} };

inline void
sendAllowedMovesForUserWithName (durak::Game &game, std::vector<GameUser> &_gameUsers, std::string const &userName)
{
  if (auto gameUserItr = ranges::find_if (_gameUsers, [&userName] (auto const &gameUser) { return gameUser._user->accountName.value () == userName; }); gameUserItr != _gameUsers.end ())
    {
      gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ calculateAllowedMoves (game, game.getRoleForName (userName)) }));
    }
}

auto const userReloggedInChillState = [] (userRelogged const &userReloggedEv, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [userName = userReloggedEv.accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == userName; }); gameUserItr != _gameUsers.end ())
    {
      gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ calculateAllowedMoves (game, game.getRoleForName (userReloggedEv.accountName)) }));
    }
};
auto const userReloggedInAskDef = [] (userRelogged const &userReloggedEv, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (game.getRoleForName (userReloggedEv.accountName) == durak::PlayerRole::defend)
    {
      if (auto gameUserItr = ranges::find_if (_gameUsers, [userName = userReloggedEv.accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == userName; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AnswerDefendWantsToTakeCardsYes, shared_class::Move::AnswerDefendWantsToTakeCardsNo } }));
        }
    }
};
auto const userReloggedInAttackAssist = [] (userRelogged const &userReloggedEv, durak::Game &game, std::vector<GameUser> &_gameUsers, PassAttackAndAssist &passAttackAndAssist) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [userName = userReloggedEv.accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == userName; }); gameUserItr != _gameUsers.end ())
    {
      if ((not passAttackAndAssist.assist && game.getRoleForName (userReloggedEv.accountName) == durak::PlayerRole::assistAttacker) || (not passAttackAndAssist.attack && game.getRoleForName (userReloggedEv.accountName) == durak::PlayerRole::attack))
        {
          auto allowedMoves = calculateAllowedMoves (game, game.getRoleForName (userReloggedEv.accountName));
          if (ranges::find_if (allowedMoves, [] (auto allowedMove) { return allowedMove == shared_class::Move::AddCards; }) != allowedMoves.end ())
            {
              gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards, shared_class::Move::AddCards } }));
            }
          else
            {
              gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards } }));
            }
        }
    }
};

auto const sendAllowedMovesBlockDef = [] (durak::Game &game, std::vector<GameUser> &_gameUsers) { sendAvailableMoves (game, _gameUsers, blockDef); };
auto const roundStartSendAllowedMovesAndGameData = [] (durak::Game &game, std::vector<GameUser> &_gameUsers) {
  sendGameDataToAccountsInGame (game, _gameUsers);
  sendAvailableMoves (game, _gameUsers, blockEverythingExceptStartAttack);
};

auto const resumeTimerHandler = [] (resumeTimer const &resumeTimerEv, durak::Game &game, std::vector<GameUser> &_gameUsers, std::function<void ()> gameOverCallback) {
  ranges::for_each (_gameUsers, [&game, &_gameUsers, playersToResume = resumeTimerEv.playersToResume, gameOverCallback] (auto &gameUser) {
    if (ranges::find (playersToResume, gameUser._user->accountName.value ()) != playersToResume.end ())
      {
        std::cout << " user:" << gameUser._user->accountName.value () << std::endl;
        if (gameUser._pausedTime)
          {
            gameUser._timer->expires_after (gameUser._pausedTime.value ());
          }
        else
          {
            std::cout << "tried to resume but _pausedTime has no value" << std::endl;
            // abort ();
          }
        gameUser._pausedTime = {};
        co_spawn (
            gameUser._timer->get_executor (), [_timer = gameUser._timer, accountName = gameUser._user->accountName.value (), &game, &_gameUsers, gameOverCallback] () { return runTimer (_timer, accountName, game, _gameUsers, gameOverCallback); }, boost::asio::detached);
      }
  });
};

auto const isDefendingPlayer = [] (defend const &defendEv, durak::Game &game) { return game.getRoleForName (defendEv.playerName) == durak::PlayerRole::defend; };
auto const setAttackAnswer = [] (attackPass const &attackPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer> process_event) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = attackPassEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto &sendingUserMsgQueue = gameUserItr->_user->msgQueue;
      if (not passAttackAndAssist.attack)
        {
          if (game.getRoleForName (attackPassEv.playerName) == durak::PlayerRole::attack)
            {
              passAttackAndAssist.attack = true;
              process_event (pauseTimer{ { attackPassEv.playerName } });
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ {} }));
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
auto const setAssistAnswer = [] (assistPass const &assistPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer> process_event) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = assistPassEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto &sendingUserMsgQueue = gameUserItr->_user->msgQueue;
      if (not passAttackAndAssist.assist)
        {
          if (game.getRoleForName (assistPassEv.playerName) == durak::PlayerRole::assistAttacker)
            {
              passAttackAndAssist.assist = true;
              process_event (pauseTimer{ { assistPassEv.playerName } });
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ {} }));
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsSuccess{}));
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
      roundStartSendAllowedMovesAndGameData (game, _gameUsers);
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

auto const startAskDef = [] (durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer, resumeTimer> process_event) {
  if (auto defendingPlayer = game.getDefendingPlayer ())
    {
      process_event (resumeTimer{ { defendingPlayer->id } });
      if (auto gameUserItr = ranges::find_if (_gameUsers, [&defendingPlayer] (auto const &gameUser) { return gameUser._user->accountName.value () == defendingPlayer->id; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCards{}));
          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AnswerDefendWantsToTakeCardsYes, shared_class::Move::AnswerDefendWantsToTakeCardsNo } }));
        }
      if (auto attackingPlayer = game.getAttackingPlayer ())
        {
          process_event (pauseTimer{ { attackingPlayer->id } });
        }
      if (auto assistingPlayer = game.getAssistingPlayer ())
        {
          process_event (pauseTimer{ { assistingPlayer->id } });
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

auto const startAskAttackAndAssist = [] (PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer, nextRoundTimer, resumeTimer> process_event) {
  if (auto defendingPlayer = game.getDefendingPlayer ())
    {
      process_event (pauseTimer{ { defendingPlayer->id } });
    }
  passAttackAndAssist = PassAttackAndAssist{};
  if (auto attackingPlayer = game.getAttackingPlayer (); attackingPlayer && not attackingPlayer->getCards ().empty ())
    {
      process_event (resumeTimer{ { attackingPlayer->id } });
      if (auto gameUserItr = ranges::find_if (_gameUsers, [&attackingPlayer] (auto const &gameUser) { return gameUser._user->accountName.value () == attackingPlayer->id; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards{}));
          auto allowedMoves = calculateAllowedMoves (game, durak::PlayerRole::attack);
          if (ranges::find_if (allowedMoves, [] (auto allowedMove) { return allowedMove == shared_class::Move::AddCards; }) != allowedMoves.end ())
            {
              gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards, shared_class::Move::AddCards } }));
            }
          else
            {
              gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards } }));
            }
        }
    }
  else
    {
      passAttackAndAssist.attack = true;
    }
  if (auto assistingPlayer = game.getAssistingPlayer (); assistingPlayer && not assistingPlayer->getCards ().empty ())
    {
      process_event (resumeTimer{ { assistingPlayer->id } });
      if (auto gameUserItr = ranges::find_if (_gameUsers, [&assistingPlayer] (auto const &gameUser) { return gameUser._user->accountName.value () == assistingPlayer->id; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards{}));
          auto allowedMoves = calculateAllowedMoves (game, durak::PlayerRole::assistAttacker);
          if (ranges::find_if (allowedMoves, [] (auto allowedMove) { return allowedMove == shared_class::Move::AddCards; }) != allowedMoves.end ())
            {
              gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards, shared_class::Move::AddCards } }));
            }
          else
            {
              gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards } }));
            }
        }
    }
  else
    {
      passAttackAndAssist.assist = true;
    }
  if (passAttackAndAssist.assist && passAttackAndAssist.attack)
    {
      game.nextRound (true);
      roundStartSendAllowedMovesAndGameData (game, _gameUsers);
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

auto const setAttackPass = [] (attackPass const &attackPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer> process_event) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = attackPassEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      // TODO think about joining this lambda and setAssistPass they do the same things we can make a function which gets called from setAttackPass and setAssistPass
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
                  process_event (pauseTimer{ { attackPassEv.playerName } });
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{}));
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

auto const setAssistPass = [] (assistPass const &assistPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer> process_event) {
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
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{}));
                  process_event (pauseTimer{ { assistPassEv.playerName } });
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

auto const handleDefendSuccess = [] (defendAnswerNo const &defendAnswerNoEv, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = defendAnswerNoEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto &sendingUserMsgQueue = gameUserItr->_user->msgQueue;
      if (game.getRoleForName (defendAnswerNoEv.playerName) == durak::PlayerRole::defend)
        {
          game.nextRound (false);
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
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCardsAnswerSuccess{}));
              roundStartSendAllowedMovesAndGameData (game, _gameUsers);
            }
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
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ {} }));
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

auto const doAttack = [] (PassAttackAndAssist &passAttackAndAssist, attack const &attackEv, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<resumeTimer, pauseTimer> process_event, bool isChill) {
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
                  if (isChill)
                    {
                      sendGameDataToAccountsInGame (game, _gameUsers);
                      sendAvailableMoves (game, _gameUsers);
                    }
                  else
                    {
                      sendGameDataToAccountsInGame (game, _gameUsers);
                      auto allowedMovesAttack = calculateAllowedMoves (game, durak::PlayerRole::attack);
                      if (ranges::find_if (allowedMovesAttack, [] (auto allowedMove) { return allowedMove == shared_class::Move::AddCards; }) != allowedMovesAttack.end ())
                        {
                          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards, shared_class::Move::AddCards } }));
                        }
                      else
                        {
                          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards } }));
                        }

                      if (auto assistingPlayer = game.getAssistingPlayer ())
                        {
                          auto allowedMovesAssist = calculateAllowedMoves (game, durak::PlayerRole::assistAttacker);
                          if (auto assistUserItr = ranges::find_if (_gameUsers, [accountName = assistingPlayer->id] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
                            {
                              if (ranges::find_if (allowedMovesAssist, [] (auto allowedMove) { return allowedMove == shared_class::Move::AddCards; }) != allowedMovesAssist.end ())
                                {
                                  assistUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards, shared_class::Move::AddCards } }));
                                }
                              else
                                {
                                  assistUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards } }));
                                }
                            }
                          process_event (resumeTimer{ { assistingPlayer->id } });
                        }
                    }
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
                  if (auto defendingPlayer = game.getDefendingPlayer ())
                    {
                      process_event (resumeTimer{ { defendingPlayer->id } });
                    }
                  if (auto attackingPlayer = game.getAttackingPlayer ())
                    {
                      process_event (pauseTimer{ { attackingPlayer->id } });
                    }
                  sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackSuccess{}));
                  sendGameDataToAccountsInGame (game, _gameUsers);
                  sendAvailableMoves (game, _gameUsers);
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
                  if (isChill)
                    {
                      sendGameDataToAccountsInGame (game, _gameUsers);
                      sendAvailableMoves (game, _gameUsers);
                    }
                  else
                    {
                      sendGameDataToAccountsInGame (game, _gameUsers);
                      auto allowedMovesAssist = calculateAllowedMoves (game, durak::PlayerRole::assistAttacker);
                      if (ranges::find_if (allowedMovesAssist, [] (auto allowedMove) { return allowedMove == shared_class::Move::AddCards; }) != allowedMovesAssist.end ())
                        {
                          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards, shared_class::Move::AddCards } }));
                        }
                      else
                        {
                          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards } }));
                        }

                      if (auto attackingPlayer = game.getAttackingPlayer ())
                        {
                          if (auto attackingUser = ranges::find_if (_gameUsers, [accountName = attackingPlayer->id] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
                            {
                              auto allowedMovesAttack = calculateAllowedMoves (game, durak::PlayerRole::attack);
                              if (ranges::find_if (allowedMovesAttack, [] (auto allowedMove) { return allowedMove == shared_class::Move::AddCards; }) != allowedMovesAttack.end ())
                                {
                                  attackingUser->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards, shared_class::Move::AddCards } }));
                                }
                              else
                                {
                                  attackingUser->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards } }));
                                }
                            }
                          process_event (resumeTimer{ { attackingPlayer->id } });
                        }
                    }
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

auto const doAttackChill = [] (PassAttackAndAssist &passAttackAndAssist, attack const &attackEv, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<resumeTimer, pauseTimer> process_event) { doAttack (passAttackAndAssist, attackEv, game, _gameUsers, process_event, true); };

auto const doAttackAskAttackAndAssist = [] (PassAttackAndAssist &passAttackAndAssist, attack const &attackEv, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<resumeTimer, pauseTimer> process_event) { doAttack (passAttackAndAssist, attackEv, game, _gameUsers, process_event, false); };

auto const doDefend = [] (defend const &defendEv, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<resumeTimer, pauseTimer> process_event) {
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
              sendAvailableMoves (game, _gameUsers);
              if (game.countOfNotBeatenCardsOnTable () == 0)
                {
                  process_event (pauseTimer{ { defendEv.playerName } });
                  if (auto attackingPlayer = game.getAttackingPlayer (); attackingPlayer && not attackingPlayer->getCards ().empty ())
                    {
                      process_event (resumeTimer{ { attackingPlayer->id } });
                    }
                  if (auto assistingPlayer = game.getAssistingPlayer (); assistingPlayer && not assistingPlayer->getCards ().empty ())
                    {
                      process_event (resumeTimer{ { assistingPlayer->id } });
                    }
                }
            }
          else
            {
              sendingUserMsgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendError{ "Error while defending " + fmt::format ("CardToBeat: {},{} vs. Card: {},{}", defendEv.cardToBeat.value, magic_enum::enum_name (defendEv.cardToBeat.type), defendEv.card.value, magic_enum::enum_name (defendEv.card.type)) }));
            }
        }
    }
};

auto const blockOnlyDef = [] (durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto defendingPlayer = game.getDefendingPlayer ())
    {
      if (auto gameUserItr = ranges::find_if (_gameUsers, [&defendingPlayer] (auto const &gameUser) { return gameUser._user->accountName.value () == defendingPlayer->id; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ {} }));
        }
    }
};

struct PassMachine
{
  // TODO rework this passing server should send moves and client should show them. use tests to check whats wrong and so on
  // TODO passing not allowed for player if defend  does not beat a card and takes cards and one player passes the other player plays a card than passes some how the other player can not pass again.

  auto
  operator() () const
  {
    using namespace boost::sml;
    return make_transition_table (
        // clang-format off
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
* "doNotStartAtConstruction"_s  + event<start>                                            /roundStartSendAllowedMovesAndGameData                                            = state<Chill>    
, state<Chill>                  + on_entry<_>                                             /(resetPassStateMachineData,process (nextRoundTimer{}))           
, state<Chill>                  + event<askDef>                                                                                       = state<AskDef>
, state<Chill>                  + event<askAttackAndAssist>                                                                           = state<AskAttackAndAssist>
, state<Chill>                  + event<attackPass>                                       /(setAttackPass,checkData)
, state<Chill>                  + event<assistPass>                                       /(setAssistPass,checkData)
, state<Chill>                  + event<defendPass>                                       / handleDefendPass                                         
, state<Chill>                  + event<attack>                                           / doAttackChill 
, state<Chill>                  + event<defend>                 [isDefendingPlayer]       / doDefend 
, state<Chill>                  + event<userRelogged>                                     / (userReloggedInChillState,sendTimer)
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
, state<AskDef>                 + on_entry<_>                                             / startAskDef
, state<AskDef>                 + event<defendAnswerYes>                                  / blockOnlyDef                                            = state<AskAttackAndAssist>
, state<AskDef>                 + event<defendAnswerNo>                                   / handleDefendSuccess                         = state<Chill>
, state<AskDef>                 + event<userRelogged>                                     / (userReloggedInAskDef,sendTimer)
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
, state<AskAttackAndAssist>     + on_entry<_>                                             / startAskAttackAndAssist
, state<AskAttackAndAssist>     + event<attackPass>                                       /(setAttackAnswer,checkAttackAndAssistAnswer)
, state<AskAttackAndAssist>     + event<assistPass>                                       /(setAssistAnswer,checkAttackAndAssistAnswer)
, state<AskAttackAndAssist>     + event<attack>                                           / doAttackAskAttackAndAssist 
, state<AskAttackAndAssist>     + event<chill>                                                                                          =state<Chill>
, state<AskAttackAndAssist>     + event<userRelogged>                                     / (userReloggedInAttackAssist,sendTimer)
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
,*"leaveGameHandler"_s          + event<leaveGame>                                        / userLeftGame                                
,*"timerHandler"_s              + event<initTimer>              [timerActive]             / (initTimerHandler,sendTimer)
, "timerHandler"_s              + event<nextRoundTimer>         [timerActive]             / (nextRoundTimerHandler,sendTimer)
, "timerHandler"_s              + event<pauseTimer>             [timerActive]             / (pauseTimerHandler,sendTimer)
, "timerHandler"_s              + event<resumeTimer>            [timerActive]             / (resumeTimerHandler,sendTimer)

// clang-format on   
    );
  }
};

typedef boost::sml::sm<PassMachine, boost::sml::logger<my_logger>,boost::sml::process_queue<std::queue>> DurakStateMachine;



#endif /* EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4 */
