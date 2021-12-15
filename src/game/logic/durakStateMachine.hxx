#ifndef EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4
#define EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4
#include "durak/game.hxx"
#include "durak/gameData.hxx"
#include "src/database/database.hxx"
#include "src/game/gameUser.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include "src/game/logic/rating.hxx"
#include "src/server/gameLobby.hxx"
#include "src/server/user.hxx"
#include "src/util.hxx"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/sml.hpp>
#include <chrono>
#include <cmath>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <cstddef>
#include <cstdlib>
#include <fmt/format.h>
#include <game_01_shared_class/serialization.hxx>
#include <iostream>
#include <magic_enum.hpp>
#include <optional>
#include <pipes/push_back.hpp>
#include <pipes/unzip.hpp>
#include <queue>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/all.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <soci/session.h>
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

auto constexpr SCORE_WON = 1;
auto const SCORE_DRAW = boost::numeric_cast<long double> (0.5);
auto constexpr RATING_CHANGE_FACTOR = 20;
void inline updateRatingWinLose (std::vector<GameUser> &_gameUsers, std::vector<database::Account> &losers, std::vector<database::Account> &winners)
{
  auto losersRatings = std::vector<size_t>{};
  losers >>= pipes::transform ([] (database::Account const &account) { return account.rating; }) >>= pipes::push_back (losersRatings);
  auto const averageRatingLosers = averageRating (losersRatings);
  auto winnersRatings = std::vector<size_t>{};
  winners >>= pipes::transform ([] (database::Account const &account) { return account.rating; }) >>= pipes::push_back (winnersRatings);
  auto const averageRatingWinners = averageRating (winnersRatings);
  auto totalRatingWon = ratingChange (averageRatingWinners, averageRatingLosers, SCORE_WON, RATING_CHANGE_FACTOR);
  auto sql = soci::session (soci::sqlite3, databaseName);
  for (auto &winner : winners)
    {
      auto const oldRating = winner.rating;
      winner.rating = winner.rating + ratingShareWinningTeam (winner.rating, winnersRatings, boost::numeric_cast<size_t> (totalRatingWon));
      confu_soci::upsertStruct (sql, winner);
      if (auto gameUser = ranges::find_if (_gameUsers, [accountName = winner.accountName] (GameUser const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUser != _gameUsers.end ())
        {
          gameUser->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::RatingChanged{ oldRating, winner.rating }));
        }
    }
  for (auto &loser : losers)
    {
      auto const oldRating = loser.rating;
      auto const newRating = loser.rating - ratingShareLosingTeam (loser.rating, losersRatings, boost::numeric_cast<size_t> (totalRatingWon));
      loser.rating = (newRating <= 0) ? 1 : newRating;
      confu_soci::upsertStruct (sql, loser);
      if (auto gameUser = ranges::find_if (_gameUsers, [accountName = loser.accountName] (GameUser const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUser != _gameUsers.end ())
        {
          gameUser->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::RatingChanged{ oldRating, loser.rating }));
        }
    }
}

void inline updateRatingDraw (std::vector<GameUser> &_gameUsers, std::vector<database::Account> &accounts)
{
  auto ratingSum = ranges::accumulate (accounts, size_t{}, [] (size_t sum, database::Account const &account) { return sum + account.rating; });
  long double totalRatingWon = 0;
  auto losers = std::vector<database::Account>{};
  auto winners = std::vector<database::Account>{};
  for (auto &account : accounts)
    {
      if (auto ratingChanged = ratingChange (account.rating, averageRating (ratingSum - account.rating, accounts.size () - 1), SCORE_DRAW, RATING_CHANGE_FACTOR); ratingChanged < 0)
        {
          totalRatingWon -= ratingChanged;
          losers.push_back (account);
        }
      else
        {
          winners.push_back (account);
        }
    }
  auto losersRatings = std::vector<size_t>{};
  losers >>= pipes::transform ([] (database::Account const &account) { return account.rating; }) >>= pipes::push_back (losersRatings);
  auto winnersRatings = std::vector<size_t>{};
  winners >>= pipes::transform ([] (database::Account const &account) { return account.rating; }) >>= pipes::push_back (winnersRatings);
  auto sql = soci::session (soci::sqlite3, databaseName);
  for (auto &winner : winners)
    {
      auto const oldRating = winner.rating;
      winner.rating = winner.rating + ratingShareWinningTeam (winner.rating, winnersRatings, boost::numeric_cast<size_t> (totalRatingWon));
      confu_soci::upsertStruct (sql, winner);
      if (auto gameUser = ranges::find_if (_gameUsers, [accountName = winner.accountName] (GameUser const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUser != _gameUsers.end ())
        {
          gameUser->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::RatingChanged{ oldRating, winner.rating }));
        }
    }
  for (auto &loser : losers)
    {
      auto const oldRating = loser.rating;
      auto const newRating = loser.rating - ratingShareLosingTeam (loser.rating, losersRatings, boost::numeric_cast<size_t> (totalRatingWon));
      loser.rating = (newRating <= 0) ? 1 : newRating;
      confu_soci::upsertStruct (sql, loser);
      if (auto gameUser = ranges::find_if (_gameUsers, [accountName = loser.accountName] (GameUser const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUser != _gameUsers.end ())
        {
          gameUser->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::RatingChanged{ oldRating, loser.rating }));
        }
    }
}

auto const handleGameOver = [] (boost::optional<durak::Player> const &durak, std::vector<GameUser> &_gameUsers, GameLobby::LobbyType lobbyType) {
  if (durak)
    {
      ranges::for_each (_gameUsers, [durak = durak->id] (auto const &gameUser) {
        if (gameUser._user->accountName == durak) gameUser._user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakGameOverLose{}));
        else
          gameUser._user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakGameOverWon{}));
      });
    }
  else
    {
      ranges::for_each (_gameUsers, [] (auto const &gameUser) { gameUser._user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakGameOverDraw{})); });
    }
  if (lobbyType == GameLobby::LobbyType::MatchMakingSystemRanked)
    {
      if (durak)
        {
          soci::session sql (soci::sqlite3, databaseName);
          auto winners = std::vector<database::Account>{};
          auto losers = std::vector<database::Account>{};
          _gameUsers >>= pipes::transform ([&sql] (auto const &gameUser) { return confu_soci::findStruct<database::Account> (sql, "accountName", gameUser._user->accountName.value ()).value (); }) >>= pipes::partition ([&durak] (database::Account const &account) { return account.accountName == durak->id; }, pipes::push_back (losers), pipes::push_back (winners));
          updateRatingWinLose (_gameUsers, losers, winners);
        }
      else
        {
          auto accountsInGame = std::vector<database::Account>{};
          soci::session sql (soci::sqlite3, databaseName);
          _gameUsers >>= pipes::transform ([&sql] (auto const &gameUser) { return confu_soci::findStruct<database::Account> (sql, "accountName", gameUser._user->accountName.value ()).value (); }) >>= pipes::push_back (accountsInGame);
          updateRatingDraw (_gameUsers, accountsInGame);
        }
    }
};

inline void
removeUserFromGame (std::string const &userToRemove, durak::Game &game, std::vector<GameUser> &_gameUsers, GameLobby::LobbyType lobbyType)
{
  if (not game.checkIfGameIsOver ())
    {
      game.removePlayer (userToRemove);
      handleGameOver (game.durak (), _gameUsers, lobbyType);
    }
}

boost::asio::awaitable<void> inline runTimer (std::shared_ptr<boost::asio::system_timer> timer, std::string const &accountName, durak::Game &game, std::vector<GameUser> &_gameUsers, std::function<void ()> gameOverCallback, GameLobby::LobbyType lobbyType)
{
  try
    {
      co_await timer->async_wait (boost::asio::use_awaitable);
      removeUserFromGame (accountName, game, _gameUsers, lobbyType);
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
      });
      ranges::for_each (_gameUsers, [&durakTimers] (auto const &gameUser) { gameUser._user->sendMessageToUser (objectToStringWithObjectName (durakTimers)); });
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

auto const nextRoundTimerHandler = [] (durak::Game &game, std::vector<GameUser> &_gameUsers, TimerOption &timerOption, boost::sml::back::process<resumeTimer, sendTimerEv> process_event) {
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
      process_event (sendTimerEv{});
    }
};

inline void
sendAllowedMovesForUserWithName (durak::Game &game, std::vector<GameUser> &_gameUsers, std::string const &userName)
{
  if (auto gameUserItr = ranges::find_if (_gameUsers, [&userName] (auto const &gameUser) { return gameUser._user->accountName.value () == userName; }); gameUserItr != _gameUsers.end ())
    {
      gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ calculateAllowedMoves (game, game.getRoleForName (userName)) }));
    }
}

auto const userReloggedInChillState = [] (userRelogged const &userReloggedEv, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [userName = userReloggedEv.accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == userName; }); gameUserItr != _gameUsers.end ())
    {
      gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ calculateAllowedMoves (game, game.getRoleForName (userReloggedEv.accountName)) }));
    }
};
auto const userReloggedInAskDef = [] (userRelogged const &userReloggedEv, durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (game.getRoleForName (userReloggedEv.accountName) == durak::PlayerRole::defend)
    {
      if (auto gameUserItr = ranges::find_if (_gameUsers, [userName = userReloggedEv.accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == userName; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AnswerDefendWantsToTakeCardsYes, shared_class::Move::AnswerDefendWantsToTakeCardsNo } }));
        }
    }
};

auto const defendsWantsToTakeCardsSendMovesToAttackOrAssist = [] (durak::Game &game, durak::PlayerRole playerRole, std::shared_ptr<User> &user) {
  auto allowedMoves = calculateAllowedMoves (game, playerRole);
  if (ranges::find_if (allowedMoves, [] (auto allowedMove) { return allowedMove == shared_class::Move::AddCards; }) != allowedMoves.end ())
    {
      user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards, shared_class::Move::AddCards } }));
    }
  else
    {
      user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AttackAssistDoneAddingCards } }));
    }
};

auto const userReloggedInAskAttackAssist = [] (userRelogged const &userReloggedEv, durak::Game &game, std::vector<GameUser> &_gameUsers, PassAttackAndAssist &passAttackAndAssist) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [userName = userReloggedEv.accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == userName; }); gameUserItr != _gameUsers.end ())
    {
      if ((not passAttackAndAssist.assist && game.getRoleForName (userReloggedEv.accountName) == durak::PlayerRole::assistAttacker) || (not passAttackAndAssist.attack && game.getRoleForName (userReloggedEv.accountName) == durak::PlayerRole::attack))
        {
          defendsWantsToTakeCardsSendMovesToAttackOrAssist (game, game.getRoleForName (userReloggedEv.accountName), gameUserItr->_user);
        }
    }
};
auto const blockEverythingExceptStartAttack = AllowedMoves{ .defend = std::vector<shared_class::Move>{}, .attack = std::vector<shared_class::Move>{ shared_class::Move::AddCards }, .assist = std::vector<shared_class::Move>{} };
auto const roundStartSendAllowedMovesAndGameData = [] (durak::Game &game, std::vector<GameUser> &_gameUsers) {
  sendGameDataToAccountsInGame (game, _gameUsers);
  sendAvailableMoves (game, _gameUsers, blockEverythingExceptStartAttack);
};

auto const resumeTimerHandler = [] (resumeTimer const &resumeTimerEv, durak::Game &game, std::vector<GameUser> &_gameUsers, std::function<void ()> gameOverCallback, GameLobby::LobbyType lobbyType) {
  ranges::for_each (_gameUsers, [&game, &_gameUsers, playersToResume = resumeTimerEv.playersToResume, gameOverCallback, lobbyType] (auto &gameUser) {
    if (ranges::find (playersToResume, gameUser._user->accountName.value ()) != playersToResume.end ())
      {
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
            gameUser._timer->get_executor (), [_timer = gameUser._timer, accountName = gameUser._user->accountName.value (), &game, &_gameUsers, gameOverCallback, lobbyType] () { return runTimer (_timer, accountName, game, _gameUsers, gameOverCallback, lobbyType); }, boost::asio::detached);
      }
  });
};

auto const isDefendingPlayer = [] (defend const &defendEv, durak::Game &game) { return game.getRoleForName (defendEv.playerName) == durak::PlayerRole::defend; };
auto const isNotFirstRound = [] (durak::Game &game) { return game.getRound () > 1; };
auto const setAttackAnswer = [] (attackPass const &attackPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer, sendTimerEv> process_event) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = attackPassEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {

      if (not passAttackAndAssist.attack)
        {
          if (game.getRoleForName (attackPassEv.playerName) == durak::PlayerRole::attack)
            {
              passAttackAndAssist.attack = true;
              process_event (pauseTimer{ { attackPassEv.playerName } });
              process_event (sendTimerEv{});
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ {} }));
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsSuccess{}));
            }
          else
            {
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ "role is not attack" }));
            }
        }
      else
        {
          gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ "pass already set" }));
        }
    }
};
auto const setAssistAnswer = [] (assistPass const &assistPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer, sendTimerEv> process_event) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = assistPassEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      if (not passAttackAndAssist.assist)
        {
          if (game.getRoleForName (assistPassEv.playerName) == durak::PlayerRole::assistAttacker)
            {
              passAttackAndAssist.assist = true;
              process_event (pauseTimer{ { assistPassEv.playerName } });
              process_event (sendTimerEv{});
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ {} }));
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsSuccess{}));
            }
          else
            {
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ "role is not assist" }));
            }
        }
      else
        {
          gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoneAddingCardsError{ "pass already set" }));
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

auto const checkAttackAndAssistAnswer = [] (PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<chill, nextRoundTimer, sendTimerEv> process_event, GameLobby::LobbyType lobbyType) {
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
      if (game.checkIfGameIsOver ())
        {
          handleGameOver (game.durak (), _gameUsers, lobbyType);
        }
      process_event (chill{});
    }
};

auto const startAskDef = [] (durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer, resumeTimer, sendTimerEv> process_event) {
  if (auto defendingPlayer = game.getDefendingPlayer ())
    {
      process_event (resumeTimer{ { defendingPlayer->id } });
      if (auto gameUserItr = ranges::find_if (_gameUsers, [&defendingPlayer] (auto const &gameUser) { return gameUser._user->accountName.value () == defendingPlayer->id; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCards{}));
          gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ { shared_class::Move::AnswerDefendWantsToTakeCardsYes, shared_class::Move::AnswerDefendWantsToTakeCardsNo } }));
        }
      if (auto attackingPlayer = game.getAttackingPlayer ())
        {
          process_event (pauseTimer{ { attackingPlayer->id } });
        }
      if (auto assistingPlayer = game.getAssistingPlayer ())
        {
          process_event (pauseTimer{ { assistingPlayer->id } });
        }
      process_event (sendTimerEv{});
    }
};

auto const userLeftGame = [] (leaveGame const &leaveGameEv, durak::Game &game, std::vector<GameUser> &_gameUsers, GameLobby::LobbyType lobbyType) {
  removeUserFromGame (leaveGameEv.accountName, game, _gameUsers, lobbyType);
  ranges::for_each (_gameUsers, [] (auto const &gameUser) { gameUser._timer->cancel (); });
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = leaveGameEv.accountName] (auto const &gameUser) { return gameUser._user->accountName == accountName; }); gameUserItr != _gameUsers.end ())
    {
      _gameUsers.erase (gameUserItr);
    }
};

auto const startAskAttackAndAssist = [] (PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer, nextRoundTimer, resumeTimer, sendTimerEv> process_event, GameLobby::LobbyType lobbyType) {
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
          gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards{}));
          defendsWantsToTakeCardsSendMovesToAttackOrAssist (game, durak::PlayerRole::attack, gameUserItr->_user);
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
          gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendWantsToTakeCardsFromTableDoYouWantToAddCards{}));
          defendsWantsToTakeCardsSendMovesToAttackOrAssist (game, durak::PlayerRole::assistAttacker, gameUserItr->_user);
        }
    }
  else
    {
      passAttackAndAssist.assist = true;
    }
  if (passAttackAndAssist.assist && passAttackAndAssist.attack)
    {
      game.nextRound (true);
      if (game.checkIfGameIsOver ())
        {
          handleGameOver (game.durak (), _gameUsers, lobbyType);
        }
    }
  else
    {
      process_event (sendTimerEv{});
    }
};

auto const doPass = [] (std::string const &playerName, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer, sendTimerEv> process_event) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [&playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == playerName; }); gameUserItr != _gameUsers.end ())
    {
      auto playerRole = game.getRoleForName (playerName);
      if (game.getAttackStarted ())
        {
          if (game.countOfNotBeatenCardsOnTable () == 0)
            {
              if (playerRole == durak::PlayerRole::attack || playerRole == durak::PlayerRole::assistAttacker)
                {
                  if (playerRole == durak::PlayerRole::attack)
                    {
                      passAttackAndAssist.attack = true;
                      gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAttackPassSuccess{}));
                    }
                  else
                    {
                      passAttackAndAssist.assist = true;
                      gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAssistPassSuccess{}));
                    }
                  gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAllowedMoves{}));
                  process_event (pauseTimer{ { playerName } });
                  process_event (sendTimerEv{});
                }
              else
                {
                  gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAttackPassError{ "account role is not attack or assist: " + playerName }));
                }
            }
          else
            {
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAttackPassError{ "there are not beaten cards on the table" }));
            }
        }
      else
        {
          gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAttackPassError{ "can not pass if attack is not started" }));
        }
    }
};
auto const setAttackPass = [] (attackPass const &attackPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer, sendTimerEv> process_event) { doPass (attackPassEv.playerName, passAttackAndAssist, game, _gameUsers, process_event); };
auto const setAssistPass = [] (assistPass const &assistPassEv, PassAttackAndAssist &passAttackAndAssist, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<pauseTimer, sendTimerEv> process_event) { doPass (assistPassEv.playerName, passAttackAndAssist, game, _gameUsers, process_event); };

auto const handleDefendSuccess = [] (defendAnswerNo const &defendAnswerNoEv, durak::Game &game, std::vector<GameUser> &_gameUsers, GameLobby::LobbyType lobbyType) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = defendAnswerNoEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      if (game.getRoleForName (defendAnswerNoEv.playerName) == durak::PlayerRole::defend)
        {
          game.nextRound (false);
          if (game.checkIfGameIsOver ())
            {
              handleGameOver (game.durak (), _gameUsers, lobbyType);
            }
          else
            {
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCardsAnswerSuccess{}));
            }
        }
      else
        {
          gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCardsAnswerError{ "account role is not defend: " + defendAnswerNoEv.playerName }));
        }
    }
};

auto const handleDefendPass = [] (defendPass const &defendPassEv, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<askAttackAndAssist> process_event) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = defendPassEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      if (game.getRoleForName (defendPassEv.playerName) == durak::PlayerRole::defend)
        {
          if (game.getAttackStarted ())
            {
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendPassSuccess{}));
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ {} }));
              process_event (askAttackAndAssist{});
            }
          else
            {
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendPassError{ "attack is not started" }));
            }
        }
      else
        {
          gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendPassError{ "account role is not defiend: " + defendPassEv.playerName }));
        }
    }
};

auto const resetPassStateMachineData = [] (PassAttackAndAssist &passAttackAndAssist) { passAttackAndAssist = PassAttackAndAssist{}; };

auto const tryToAttackAndInformOtherPlayers = [] (PassAttackAndAssist &passAttackAndAssist, attack const &attackEv, durak::PlayerRole playerRole, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<resumeTimer, pauseTimer, sendTimerEv> process_event, bool isChill, std::shared_ptr<User> user) {
  if (game.playerAssists (playerRole, attackEv.cards))
    {
      user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAttackSuccess{}));
      if (isChill)
        {
          if (auto defendingPlayer = game.getDefendingPlayer ())
            {
              process_event (resumeTimer{ { defendingPlayer->id } });
            }
          if (auto attackingPlayer = game.getAttackingPlayer ())
            {
              process_event (pauseTimer{ { attackingPlayer->id } });
            }
          if (auto assistingPlayer = game.getAssistingPlayer ())
            {
              process_event (pauseTimer{ { assistingPlayer->id } });
            }
          sendGameDataToAccountsInGame (game, _gameUsers);
          sendAvailableMoves (game, _gameUsers);
          process_event (sendTimerEv{});
        }
      else
        {
          sendGameDataToAccountsInGame (game, _gameUsers);
          auto otherPlayerRole = (playerRole == durak::PlayerRole::attack) ? durak::PlayerRole::assistAttacker : durak::PlayerRole::attack;
          defendsWantsToTakeCardsSendMovesToAttackOrAssist (game, playerRole, user);
          if (auto otherPlayer = (otherPlayerRole == durak::PlayerRole::attack) ? game.getAttackingPlayer () : game.getAssistingPlayer ())
            {
              if (auto otherPlayerItr = ranges::find_if (_gameUsers, [otherPlayerName = otherPlayer->id] (auto const &gameUser) { return gameUser._user->accountName.value () == otherPlayerName; }); otherPlayerItr != _gameUsers.end ())
                {
                  defendsWantsToTakeCardsSendMovesToAttackOrAssist (game, otherPlayerRole, otherPlayerItr->_user);
                }
              process_event (resumeTimer{ { otherPlayer->id } });
              process_event (sendTimerEv{});
            }
        }
      passAttackAndAssist = PassAttackAndAssist{};
    }
};

auto const doAttack = [] (PassAttackAndAssist &passAttackAndAssist, attack const &attackEv, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<resumeTimer, pauseTimer, sendTimerEv> process_event, bool isChill) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = attackEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {
      auto playerRole = game.getRoleForName (attackEv.playerName);
      if (not game.getAttackStarted () && playerRole == durak::PlayerRole::attack)
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
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAttackSuccess{}));
              sendGameDataToAccountsInGame (game, _gameUsers);
              sendAvailableMoves (game, _gameUsers);
              process_event (sendTimerEv{});
              passAttackAndAssist = PassAttackAndAssist{};
            }
          else
            {
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAttackError{ "not allowed to play cards" }));
            }
        }
      else
        {
          tryToAttackAndInformOtherPlayers (passAttackAndAssist, attackEv, playerRole, game, _gameUsers, process_event, isChill, gameUserItr->_user);
        }
    }
};

auto const doAttackChill = [] (PassAttackAndAssist &passAttackAndAssist, attack const &attackEv, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<resumeTimer, pauseTimer, sendTimerEv> process_event) { doAttack (passAttackAndAssist, attackEv, game, _gameUsers, process_event, true); };

auto const doAttackAskAttackAndAssist = [] (PassAttackAndAssist &passAttackAndAssist, attack const &attackEv, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<resumeTimer, pauseTimer, sendTimerEv> process_event) { doAttack (passAttackAndAssist, attackEv, game, _gameUsers, process_event, false); };

auto const doDefend = [] (defend const &defendEv, durak::Game &game, std::vector<GameUser> &_gameUsers, boost::sml::back::process<resumeTimer, pauseTimer, sendTimerEv> process_event) {
  if (auto gameUserItr = ranges::find_if (_gameUsers, [accountName = defendEv.playerName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }); gameUserItr != _gameUsers.end ())
    {

      auto playerRole = game.getRoleForName (defendEv.playerName);
      if (playerRole == durak::PlayerRole::defend)
        {
          if (game.playerDefends (defendEv.cardToBeat, defendEv.card))
            {
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendSuccess{}));
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
                  process_event (sendTimerEv{});
                }
            }
          else
            {
              gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakDefendError{ "Error while defending " + fmt::format ("CardToBeat: {},{} vs. Card: {},{}", defendEv.cardToBeat.value, magic_enum::enum_name (defendEv.cardToBeat.type), defendEv.card.value, magic_enum::enum_name (defendEv.card.type)) }));
            }
        }
    }
};

auto const blockOnlyDef = [] (durak::Game &game, std::vector<GameUser> &_gameUsers) {
  if (auto defendingPlayer = game.getDefendingPlayer ())
    {
      if (auto gameUserItr = ranges::find_if (_gameUsers, [&defendingPlayer] (auto const &gameUser) { return gameUser._user->accountName.value () == defendingPlayer->id; }); gameUserItr != _gameUsers.end ())
        {
          gameUserItr->_user->sendMessageToUser (objectToStringWithObjectName (shared_class::DurakAllowedMoves{ {} }));
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
* "doNotStartAtConstruction"_s  + event<start>                                            /(roundStartSendAllowedMovesAndGameData, process (sendTimerEv{}))                                      = state<Chill>    
, state<Chill>                  + on_entry<_>                   [isNotFirstRound]         /(resetPassStateMachineData,process (nextRoundTimer{}),roundStartSendAllowedMovesAndGameData)           
, state<Chill>                  + event<askDef>                                                                                                                           = state<AskDef>
, state<Chill>                  + event<askAttackAndAssist>                                                                                                               = state<AskAttackAndAssist>
, state<Chill>                  + event<attackPass>                                       /(setAttackPass,checkData)
, state<Chill>                  + event<assistPass>                                       /(setAssistPass,checkData)
, state<Chill>                  + event<defendPass>                                       / handleDefendPass                                         
, state<Chill>                  + event<attack>                                           / doAttackChill 
, state<Chill>                  + event<defend>                 [isDefendingPlayer]       / doDefend 
, state<Chill>                  + event<userRelogged>                                     / (userReloggedInChillState)
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
, state<AskDef>                 + on_entry<_>                                             / startAskDef
, state<AskDef>                 + event<defendAnswerYes>                                  / blockOnlyDef                                                                  = state<AskAttackAndAssist>
, state<AskDef>                 + event<defendAnswerNo>                                   / handleDefendSuccess                                                           = state<Chill>
, state<AskDef>                 + event<userRelogged>                                     / (userReloggedInAskDef)
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
, state<AskAttackAndAssist>     + on_entry<_>                                             / startAskAttackAndAssist
, state<AskAttackAndAssist>     + event<attackPass>                                       /(setAttackAnswer,checkAttackAndAssistAnswer)
, state<AskAttackAndAssist>     + event<assistPass>                                       /(setAssistAnswer,checkAttackAndAssistAnswer)
, state<AskAttackAndAssist>     + event<attack>                                           / doAttackAskAttackAndAssist 
, state<AskAttackAndAssist>     + event<chill>                                                                                                                            =state<Chill>
, state<AskAttackAndAssist>     + event<userRelogged>                                     / (userReloggedInAskAttackAssist)
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/      
,*"leaveGameHandler"_s          + event<leaveGame>                                        / userLeftGame                                
,*"timerHandler"_s              + event<initTimer>              [timerActive]             / (initTimerHandler)
, "timerHandler"_s              + event<nextRoundTimer>         [timerActive]             / (nextRoundTimerHandler)
, "timerHandler"_s              + event<pauseTimer>             [timerActive]             / (pauseTimerHandler)
, "timerHandler"_s              + event<resumeTimer>            [timerActive]             / (resumeTimerHandler)
, "timerHandler"_s              + event<sendTimerEv>            [timerActive]             / (sendTimer)

// clang-format on   
    );
  }
};

typedef boost::sml::sm<PassMachine, boost::sml::logger<my_logger>,boost::sml::process_queue<std::queue>> DurakStateMachine;



#endif /* EDA48A4C_C02A_4C25_B6A5_0D0EF497AFC4 */
