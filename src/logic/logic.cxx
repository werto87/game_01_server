#include "src/logic/logic.hxx"
#include "src/database/database.hxx"
#include "src/game/logic/durakStateMachineState.hxx"
#include "src/game/logic/gameMachine.hxx"
#include "src/pw_hash/passwordHash.hxx"
#include "src/server/gameLobby.hxx"
#include "src/util.hxx"
#include <algorithm>
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/fusion/include/pair.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/support/pair.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/type_index.hpp>
#include <boost/uuid/uuid.hpp>
#include <cmath>
#include <confu_json/confu_json.hxx>
#include <crypt.h>
#include <cstddef>
#include <cstdlib>
#include <durak/game.hxx>
#include <durak/gameData.hxx>
#include <durak/gameOption.hxx>
#include <fmt/core.h>
#include <game_01_shared_class/serialization.hxx>
#include <iostream>
#include <iterator>
#include <numeric>
#include <optional>
#include <pipes/filter.hpp>
#include <pipes/pipes.hpp>
#include <pipes/push_back.hpp>
#include <pipes/transform.hpp>
#include <range/v3/algorithm/copy_if.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/all.hpp>
#include <range/v3/iterator/insert_iterators.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/range.hpp>
#include <range/v3/range_fwd.hpp>
#include <range/v3/view/filter.hpp>
#include <sodium.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

template <class... Ts> struct overloaded : Ts...
{
  using Ts::operator()...;
};
template <class... Ts> overloaded (Ts...) -> overloaded<Ts...>;

void
removeGameIfItIsOver (std::string const &accountName, std::list<GameMachine> &gameMachines)
{
  if (auto gameWithUser = ranges::find_if (gameMachines,
                                           [&accountName] (auto const &gameMachine) {
                                             auto accountNames = std::vector<std::string>{};
                                             ranges::transform (gameMachine.getGameUsers (), ranges::back_inserter (accountNames), [] (auto const &gameUser) { return gameUser._user->accountName.value (); });
                                             return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                           });
      gameWithUser != gameMachines.end ())
    {
      if (gameWithUser->getGame ().checkIfGameIsOver ())
        {
          gameMachines.erase (gameWithUser);
        }
    }
}

boost::asio::awaitable<void>
handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies, std::list<GameMachine> &gameMachines)
{
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () == 2)
    {
      // TODO diferentiate between logged in and not logged user actions
      auto const &typeToSearch = splitMesssage.at (0);
      auto const &objectAsString = splitMesssage.at (1);
      if (typeToSearch == "CreateAccount")
        {
          co_await createAccountAndLogin (objectAsString, io_context, user, pool, gameLobbies, gameMachines);
          user->ignoreCreateAccount = false;
          user->ignoreLogin = false;
        }
      else if (typeToSearch == "LoginAccount")
        {
          co_await loginAccount (objectAsString, io_context, users, user, pool, gameLobbies, gameMachines);
          user->ignoreCreateAccount = false;
          user->ignoreLogin = false;
        }
      else if (typeToSearch == "BroadCastMessage")
        {
          broadCastMessage (objectAsString, users, *user);
        }
      else if (typeToSearch == "JoinChannel")
        {
          joinChannel (objectAsString, user);
        }
      else if (user->accountName && typeToSearch == "LeaveChannel")
        {
          leaveChannel (objectAsString, user);
        }
      else if (typeToSearch == "LogoutAccount")
        {
          logoutAccount (user, gameLobbies, gameMachines);
        }
      else if (typeToSearch == "CreateGameLobby")
        {
          createGameLobby (objectAsString, user, gameLobbies);
        }
      else if (typeToSearch == "JoinGameLobby")
        {
          joinGameLobby (objectAsString, user, gameLobbies);
        }
      else if (typeToSearch == "SetMaxUserSizeInCreateGameLobby")
        {
          setMaxUserSizeInCreateGameLobby (objectAsString, user, gameLobbies);
        }
      else if (typeToSearch == "GameOption")
        {
          setGameOption (objectAsString, user, gameLobbies);
        }
      else if (typeToSearch == "LeaveGameLobby")
        {
          leaveGameLobby (user, gameLobbies);
        }
      else if (typeToSearch == "RelogTo")
        {
          relogTo (objectAsString, user, gameLobbies, gameMachines);
        }
      else if (typeToSearch == "LoginAccountCancel")
        {
          loginAccountCancel (user);
        }
      else if (typeToSearch == "CreateAccountCancel")
        {
          createAccountCancel (user);
        }
      else if (typeToSearch == "CreateGame")
        {
          createGame (user, gameLobbies, gameMachines, io_context);
        }
      else if (typeToSearch == "DurakAttack")
        {
          durakAttack (objectAsString, user, gameMachines);
        }
      else if (typeToSearch == "DurakDefend")
        {
          durakDefend (objectAsString, user, gameMachines);
        }
      else if (typeToSearch == "DurakDefendPass")
        {
          durakDefendPass (user, gameMachines);
        }
      else if (typeToSearch == "DurakAttackPass")
        {
          durakAttackPass (user, gameMachines);
        }
      else if (typeToSearch == "DurakAssistPass")
        {
          durakAssistPass (user, gameMachines);
        }
      else if (typeToSearch == "DurakAskDefendWantToTakeCardsAnswer")
        {
          durakAskDefendWantToTakeCardsAnswer (objectAsString, user, gameMachines);
        }
      else if (typeToSearch == "DurakLeaveGame")
        {
          durakLeaveGame (user, gameMachines);
        }
      else if (typeToSearch == "SetTimerOption")
        {
          setTimerOption (objectAsString, user, gameLobbies);
        }

      else if (typeToSearch == "JoinMatchMakingQueue")
        {

          joinMatchMakingQueue (user, gameLobbies, io_context, (stringToObject<shared_class::JoinMatchMakingQueue> (objectAsString).isRanked) ? GameLobby::LobbyType::MatchMakingSystemRanked : GameLobby::LobbyType::MatchMakingSystemUnranked);
        }

      else if (typeToSearch == "WantsToJoinGame")
        {
          wantsToJoinGame (objectAsString, user, gameLobbies, gameMachines, io_context);
        }
      else if (typeToSearch == "LeaveQuickGameQueue")
        {
          leaveMatchMakingQueue (user, gameLobbies);
        }
      else if (typeToSearch == "LoginAsGuest")
        {
          loginAsGuest (user);
        }
      else if (typeToSearch == "JoinRankedGameQueue")
        {
          joinMatchMakingQueue (user, gameLobbies, io_context, GameLobby::LobbyType::MatchMakingSystemRanked);
        }
      else
        {
          std::cout << "UnhandledMessage|{\"message\": \"" << msg << "\"}" << std::endl;
          user->msgQueue.push_back ("UnhandledMessage|{\"message\": \"" + msg + "\"}");
        }
    }
  else
    {
      std::cout << "UnhandledMessage|{\"message\": \"" << msg << "\"}" << std::endl;
      user->msgQueue.push_back ("UnhandledMessage|{\"message\": \"" + msg + "\"}");
    }
  if (user->accountName)
    {
      //  TODO this is a workaround because calling gameOver in the statemachine leads to a crash in the destructor
      //  TODO the statemachine should call gameOver

      removeGameIfItIsOver (user->accountName.value (), gameMachines);
    }
  co_return;
}

boost::asio::awaitable<void>
createAccountAndLogin (std::string objectAsString, boost::asio::io_context &io_context, std::shared_ptr<User> user, boost::asio::thread_pool &pool, std::list<GameLobby> &gameLobbies, std::list<GameMachine> &gameMachines)
{
  if (user->accountName)
    {
      logoutAccount (user, gameLobbies, gameMachines);
    }
  auto createAccountObject = stringToObject<shared_class::CreateAccount> (objectAsString);
  soci::session sql (soci::sqlite3, databaseName);
  if (confu_soci::findStruct<database::Account> (sql, "accountName", createAccountObject.accountName))
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "account already created" }));
      co_return;
    }
  else
    {
      auto hashedPw = co_await async_hash (pool, io_context, createAccountObject.password, boost::asio::use_awaitable);
      if (user->ignoreCreateAccount)
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "Canceled by User Request" }));
          co_return;
        }
      else
        {
          if (auto account = database::createAccount (createAccountObject.accountName, hashedPw))
            {
              user->accountName = account->accountName;
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountSuccess{ createAccountObject.accountName }));
              co_return;
            }
          else
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateAccountError{ createAccountObject.accountName, "account already created" }));
              co_return;
            }
        }
    }
}

boost::asio::awaitable<void>
loginAccount (std::string objectAsString, boost::asio::io_context &io_context, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, boost::asio::thread_pool &pool, std::list<GameLobby> &gameLobbies, std::list<GameMachine> &gameMachines)
{
  if (user->accountName)
    {
      logoutAccount (user, gameLobbies, gameMachines);
    }
  auto loginAccountObject = stringToObject<shared_class::LoginAccount> (objectAsString);
  soci::session sql (soci::sqlite3, databaseName);
  if (auto account = confu_soci::findStruct<database::Account> (sql, "accountName", loginAccountObject.accountName))
    {
      if (std::find_if (users.begin (), users.end (), [accountName = account->accountName] (auto const &u) { return accountName == u->accountName; }) != users.end ())
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Account already logged in" }));
          co_return;
        }
      else
        {
          if (co_await async_check_hashed_pw (pool, io_context, account->password, loginAccountObject.password, boost::asio::use_awaitable))
            {
              if (user->ignoreLogin)
                {
                  user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Canceled by User Request" }));
                  co_return;
                }
              else
                {
                  user->accountName = account->accountName;
                  if (auto gameLobbyWithUser = ranges::find_if (gameLobbies,
                                                                [accountName = user->accountName] (auto const &gameLobby) {
                                                                  auto const &accountNames = gameLobby.accountNames ();
                                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                                });
                      gameLobbyWithUser != gameLobbies.end ())
                    {
                      if (gameLobbyWithUser->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers)
                        {
                          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::WantToRelog{ loginAccountObject.accountName, "Create Game Lobby" }));
                        }
                      else
                        {
                          gameLobbyWithUser->removeUser (user);
                          if (gameLobbyWithUser->accountCount () == 0)
                            {
                              gameLobbies.erase (gameLobbyWithUser);
                            }
                          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountSuccess{ loginAccountObject.accountName }));
                        }
                      co_return;
                    }
                  else if (auto gameWithUser = ranges::find_if (gameMachines,
                                                                [accountName = user->accountName] (auto const &gameMachine) {
                                                                  auto accountNames = std::vector<std::string>{};
                                                                  ranges::transform (gameMachine.getGameUsers (), ranges::back_inserter (accountNames), [] (auto const &gameUser) { return gameUser._user->accountName.value (); });
                                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                                });
                           gameWithUser != gameMachines.end ())
                    {
                      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::WantToRelog{ loginAccountObject.accountName, "Back To Game" }));
                      co_return;
                    }
                  else
                    {
                      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountSuccess{ loginAccountObject.accountName }));
                      co_return;
                    }
                }
            }
          else
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Incorrect Username or Password" }));
              co_return;
            }
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAccountError{ loginAccountObject.accountName, "Incorrect username or password" }));
      co_return;
    }
}

void
broadCastMessage (std::string const &objectAsString, std::list<std::shared_ptr<User>> &users, User &sendingUser)
{
  auto broadCastMessageObject = stringToObject<shared_class::BroadCastMessage> (objectAsString);
  if (sendingUser.accountName)
    {
      for (auto &user : users | ranges::views::filter ([channel = broadCastMessageObject.channel, accountName = sendingUser.accountName] (auto const &user) { return user->communicationChannels.find (channel) != user->communicationChannels.end (); }))
        {
          soci::session sql (soci::sqlite3, databaseName);
          auto message = shared_class::Message{ sendingUser.accountName.value (), broadCastMessageObject.channel, broadCastMessageObject.message };
          user->msgQueue.push_back (objectToStringWithObjectName (std::move (message)));
        }
      return;
    }
  else
    {
      sendingUser.msgQueue.push_back (objectToStringWithObjectName (shared_class::BroadCastMessageError{ broadCastMessageObject.channel, "account not logged in" }));
      return;
    }
}

void
joinChannel (std::string const &objectAsString, std::shared_ptr<User> user)
{
  auto joinChannelObject = stringToObject<shared_class::JoinChannel> (objectAsString);
  if (user->accountName)
    {
      user->communicationChannels.insert (joinChannelObject.channel);
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinChannelSuccess{ joinChannelObject.channel }));
      return;
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinChannelError{ joinChannelObject.channel, { "user not logged in" } }));
      return;
    }
}

void
leaveChannel (std::string const &objectAsString, std::shared_ptr<User> user)
{
  auto leaveChannelObject = stringToObject<shared_class::LeaveChannel> (objectAsString);
  if (user->accountName)
    {
      if (user->communicationChannels.erase (leaveChannelObject.channel))
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveChannelSuccess{ leaveChannelObject.channel }));
          return;
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveChannelError{ leaveChannelObject.channel, { "channel not found" } }));
          return;
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveChannelError{ leaveChannelObject.channel, { "user not logged in" } }));
      return;
    }
}

std::optional<shared_class::GameOptionError>
errorInGameOption (durak::GameOption const &gameOption)
{
  if (gameOption.customCardDeck)
    {
      if (gameOption.customCardDeck->empty ())
        {
          return shared_class::GameOptionError{ "Empty Custom Deck is not allowed" };
        }
      else
        {
          return std::nullopt;
        }
    }
  else
    {
      if (gameOption.maxCardValue < 1)
        {
          return shared_class::GameOptionError{ "Max Card Value must be greater than 0" };
        }
      else
        {
          return std::nullopt;
        }
    }
}

void
createGame (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies, std::list<GameMachine> &gameMachines, boost::asio::io_context &io_context)
{
  if (auto gameLobbyWithUser = ranges::find_if (gameLobbies,
                                                [accountName = user->accountName] (auto const &gameLobby) {
                                                  auto const &accountNames = gameLobby.accountNames ();
                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                });
      gameLobbyWithUser != gameLobbies.end ())
    {
      if (gameLobbyWithUser->isGameLobbyAdmin (user->accountName.value ()))
        {
          if (gameLobbyWithUser->accountNames ().size () >= 2)
            {
              if (auto gameOptionError = errorInGameOption (gameLobbyWithUser->gameOption))
                {
                  user->msgQueue.push_back (objectToStringWithObjectName (gameOptionError.value ()));
                }
              else
                {
                  ranges::for_each (gameLobbyWithUser->_users, [] (auto &_user) { _user->msgQueue.push_back (objectToStringWithObjectName (shared_class::StartGame{})); });
                  auto names = std::vector<std::string>{};
                  ranges::transform (gameLobbyWithUser->_users, ranges::back_inserter (names), [] (auto const &tempUser) { return tempUser->accountName.value (); });
                  auto game = durak::Game{ std::move (names), gameLobbyWithUser->gameOption };
                  gameMachines.emplace_back (
                      game, gameLobbyWithUser->_users, io_context, gameLobbyWithUser->timerOption,
                      [accountName = user->accountName, &gameMachines] () {
                        if (auto gameWithUser = ranges::find_if (gameMachines,
                                                                 [accountName] (auto const &gameMachine) {
                                                                   auto accountNames = std::vector<std::string>{};
                                                                   ranges::transform (gameMachine.getGameUsers (), ranges::back_inserter (accountNames), [] (auto const &gameUser) { return gameUser._user->accountName.value (); });
                                                                   return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                                 });
                            gameWithUser != gameMachines.end ())
                          {
                            gameMachines.erase (gameWithUser);
                          }
                      },
                      gameLobbyWithUser->lobbyAdminType);
                  gameLobbies.erase (gameLobbyWithUser);
                }
            }
          else
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameError{ "You need atleast two user to create a game" }));
            }
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameError{ "you need to be admin in a game lobby to start a game" }));
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameError{ "Could not find a game lobby for the user" }));
    }
}

void
createGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies)
{
  auto createGameLobbyObject = stringToObject<shared_class::CreateGameLobby> (objectAsString);
  if (ranges::find_if (gameLobbies, [gameLobbyName = createGameLobbyObject.name, lobbyPassword = createGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName; }) == gameLobbies.end ())
    {
      if (auto gameLobbyWithUser = ranges::find_if (gameLobbies,
                                                    [accountName = user->accountName] (auto const &gameLobby) {
                                                      auto const &accountNames = gameLobby.accountNames ();
                                                      return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                    });
          gameLobbyWithUser != gameLobbies.end ())
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameLobbyError{ { "account has already a game lobby with the name: " + gameLobbyWithUser->name.value () } }));
          return;
        }
      else
        {
          auto &newGameLobby = gameLobbies.emplace_back (GameLobby{ createGameLobbyObject.name, createGameLobbyObject.password });
          if (newGameLobby.tryToAddUser (user))
            {
              throw std::logic_error{ "user can not join lobby which he created" };
            }
          else
            {
              auto result = std::vector<std::string>{};
              auto usersInGameLobby = shared_class::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = newGameLobby.maxUserCount ();
              usersInGameLobby.name = newGameLobby.name.value ();
              usersInGameLobby.durakGameOption = newGameLobby.gameOption;
              ranges::transform (newGameLobby.accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbySuccess{}));
              user->msgQueue.push_back (objectToStringWithObjectName (usersInGameLobby));
              return;
            }
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::CreateGameLobbyError{ { "lobby already exists with name: " + createGameLobbyObject.name } }));
      return;
    }
}

void
joinGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies)
{
  auto joinGameLobbyObject = stringToObject<shared_class::JoinGameLobby> (objectAsString);
  if (auto gameLobby = ranges::find_if (gameLobbies, [gameLobbyName = joinGameLobbyObject.name, lobbyPassword = joinGameLobbyObject.password] (auto const &_gameLobby) { return _gameLobby.name && _gameLobby.name == gameLobbyName && _gameLobby.password == lobbyPassword; }); gameLobby != gameLobbies.end ())
    {
      if (auto error = gameLobby->tryToAddUser (user))
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbyError{ joinGameLobbyObject.name, error.value () }));
          return;
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbySuccess{}));
          auto usersInGameLobby = shared_class::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobby->maxUserCount ();
          usersInGameLobby.name = gameLobby->name.value ();
          usersInGameLobby.durakGameOption = gameLobby->gameOption;
          ranges::transform (gameLobby->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
          gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
          gameLobby->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::SetTimerOption{ gameLobby->timerOption.timerType, boost::numeric_cast<int> (gameLobby->timerOption.timeAtStart.count ()), boost::numeric_cast<int> (gameLobby->timerOption.timeForEachRound.count ()) }));
          return;
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbyError{ joinGameLobbyObject.name, "wrong password name combination or lobby does not exists" }));
      return;
    }
}

void
setMaxUserSizeInCreateGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies)
{
  auto setMaxUserSizeInCreateGameLobbyObject = stringToObject<shared_class::SetMaxUserSizeInCreateGameLobby> (objectAsString);
  auto accountNameToSearch = user->accountName.value ();
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbies.end ())
    {
      if (gameLobbyWithAccount->isGameLobbyAdmin (user->accountName.value ()))
        {
          if (auto errorMessage = gameLobbyWithAccount->setMaxUserCount (setMaxUserSizeInCreateGameLobbyObject.maxUserSize))
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ errorMessage.value () }));
              return;
            }
          else
            {
              gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::MaxUserSizeInCreateGameLobby{ setMaxUserSizeInCreateGameLobbyObject.maxUserSize }));
              return;
            }
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ "you need to be admin in a game lobby to change the user size" }));
          return;
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetMaxUserSizeInCreateGameLobbyError{ "could not find a game lobby for account" }));
      return;
    }
}

void
setGameOption (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies)
{
  auto gameOption = stringToObject<durak::GameOption> (objectAsString);
  auto accountNameToSearch = user->accountName.value ();
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbies.end ())
    {
      if (gameLobbyWithAccount->isGameLobbyAdmin (user->accountName.value ()))
        {
          gameLobbyWithAccount->gameOption = gameOption;
          gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (gameOption));
          return;
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::GameOptionError{ "you need to be admin in the create game lobby to change game option" }));
          return;
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::GameOptionError{ "could not find a game lobby for account" }));
      return;
    }
}

void
setTimerOption (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies)
{
  auto setTimerOptionObject = stringToObject<shared_class::SetTimerOption> (objectAsString);
  auto accountNameToSearch = user->accountName.value ();
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbies.end ())
    {
      if (gameLobbyWithAccount->isGameLobbyAdmin (user->accountName.value ()))
        {
          using namespace std::chrono;
          gameLobbyWithAccount->timerOption = TimerOption{ setTimerOptionObject.timerType, seconds (setTimerOptionObject.timeAtStartInSeconds), seconds (setTimerOptionObject.timeForEachRoundInSeconds) };
          gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (setTimerOptionObject));
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetTimerOptionError{ "you need to be admin in a game lobby to change the timer option" }));
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetTimerOptionError{ "could not find a game lobby for account" }));
    }
}

void
leaveGameLobby (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies)
{
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbies.end ())
    {
      if (gameLobbyWithAccount->lobbyAdminType == GameLobby::LobbyType::FirstUserInLobbyUsers)
        {
          gameLobbyWithAccount->removeUser (user);
          if (gameLobbyWithAccount->accountCount () == 0)
            {
              gameLobbies.erase (gameLobbyWithAccount);
            }
          else
            {
              auto usersInGameLobby = shared_class::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
              usersInGameLobby.name = gameLobbyWithAccount->name.value ();
              usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
              ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
              gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
            }
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveGameLobbySuccess{}));
          return;
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveGameLobbyError{ "not allowed to leave a game lobby which is controlled by the matchmaking system with leave game lobby" }));
          return;
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveGameLobbyError{ "could not remove user from lobby user not found in lobby" }));
      return;
    }
}

void
relogTo (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies, std::list<GameMachine> &gameMachines)
{
  auto relogToObject = stringToObject<shared_class::RelogTo> (objectAsString);
  if (auto gameLobbyWithAccount = ranges::find_if (gameLobbies,
                                                   [accountName = user->accountName] (auto const &gameLobby) {
                                                     auto const &accountNames = gameLobby.accountNames ();
                                                     return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                   });
      gameLobbyWithAccount != gameLobbies.end ())
    {
      if (relogToObject.wantsToRelog)
        {
          gameLobbyWithAccount->relogUser (user);
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::RelogToCreateGameLobbySuccess{}));
          auto usersInGameLobby = shared_class::UsersInGameLobby{};
          usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
          usersInGameLobby.name = gameLobbyWithAccount->name.value ();
          usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
          ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::SetTimerOption{ gameLobbyWithAccount->timerOption.timerType, boost::numeric_cast<int> (gameLobbyWithAccount->timerOption.timeAtStart.count ()), boost::numeric_cast<int> (gameLobbyWithAccount->timerOption.timeForEachRound.count ()) }));
          user->msgQueue.push_back (objectToStringWithObjectName (usersInGameLobby));
          return;
        }
      else
        {
          gameLobbyWithAccount->removeUser (user);
          if (gameLobbyWithAccount->accountCount () == 0)
            {
              gameLobbies.erase (gameLobbyWithAccount);
            }
          else
            {
              auto usersInGameLobby = shared_class::UsersInGameLobby{};
              usersInGameLobby.maxUserSize = gameLobbyWithAccount->maxUserCount ();
              usersInGameLobby.name = gameLobbyWithAccount->name.value ();
              usersInGameLobby.durakGameOption = gameLobbyWithAccount->gameOption;
              ranges::transform (gameLobbyWithAccount->accountNames (), ranges::back_inserter (usersInGameLobby.users), [] (auto const &accountName) { return shared_class::UserInGameLobby{ accountName }; });
              gameLobbyWithAccount->sendToAllAccountsInGameLobby (objectToStringWithObjectName (usersInGameLobby));
              return;
            }
        }
    }
  else if (auto gameWithUser = ranges::find_if (gameMachines,
                                                [accountName = user->accountName] (auto const &gameMachine) {
                                                  auto accountNames = std::vector<std::string>{};
                                                  ranges::transform (gameMachine.getGameUsers (), ranges::back_inserter (accountNames), [] (auto const &gameUser) { return gameUser._user->accountName.value (); });
                                                  return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                });
           gameWithUser != gameMachines.end ())
    {
      if (relogToObject.wantsToRelog)
        {
          gameWithUser->relogUser (user);
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::RelogToGameSuccess{}));
          auto gameData = gameWithUser->getGame ().getGameData ();
          if (auto playerRelog = ranges::find_if (gameData.players, [accountName = user->accountName.value ()] (auto const &player) { return player.name == accountName; }); playerRelog != gameData.players.end ())
            {
              ranges::sort (playerRelog->cards, [] (auto const &card1, auto const &card2) { return card1.value () < card2.value (); });
              user->msgQueue.push_back (objectToStringWithObjectName (filterGameDataByAccountName (gameData, user->accountName.value ())));
            }
          gameWithUser->durakStateMachine.process_event (userRelogged{ user->accountName.value () });
        }
      else
        {
          gameWithUser->durakStateMachine.process_event (leaveGame{ user->accountName.value () });
          gameMachines.erase (gameWithUser);
        }
    }
  else if (relogToObject.wantsToRelog)
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::RelogToError{ "trying to reconnect into game lobby but game lobby does not exist anymore" }));
      return;
    }
  return;
}

void
durakAttack (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  auto durakAttackObject = stringToObject<shared_class::DurakAttack> (objectAsString);
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (attack{ .playerName = user->accountName.value (), .cards{ std::move (durakAttackObject.cards) } });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakDefend (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  auto durakDefendObject = stringToObject<shared_class::DurakDefend> (objectAsString);
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (defend{ .playerName = user->accountName.value (), .cardToBeat{ durakDefendObject.cardToBeat }, .card{ durakDefendObject.card } });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakAttackPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (attackPass{ .playerName = user->accountName.value () });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAttackPassError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakAssistPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (assistPass{ .playerName = user->accountName.value () });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAssistPassError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakAskDefendWantToTakeCardsAnswer (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  auto durakAskDefendWantToTakeCardsAnswerObject = stringToObject<shared_class::DurakAskDefendWantToTakeCardsAnswer> (objectAsString);
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      if (durakAskDefendWantToTakeCardsAnswerObject.answer)
        {
          gameMachine->durakStateMachine.process_event (defendAnswerYes{ .playerName = user->accountName.value () });
        }
      else
        {
          gameMachine->durakStateMachine.process_event (defendAnswerNo{ .playerName = user->accountName.value () });
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakAskDefendWantToTakeCardsAnswerError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
loginAccountCancel (std::shared_ptr<User> user)
{
  if (not user->accountName)
    {
      user->ignoreLogin = true;
    }
}

void
createAccountCancel (std::shared_ptr<User> user)
{
  if (not user->accountName)
    {
      user->ignoreCreateAccount = true;
    }
}
void
durakDefendPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (defendPass{ .playerName = user->accountName.value () });
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakDefendPassError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}

void
durakLeaveGame (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines)
{
  if (auto gameMachine = ranges::find_if (gameMachines, [accountName = user->accountName.value ()] (GameMachine const &_game) { return ranges::find_if (_game.getGameUsers (), [&accountName] (auto const &gameUser) { return gameUser._user->accountName.value () == accountName; }) != _game.getGameUsers ().end (); }); gameMachine != gameMachines.end ())
    {
      gameMachine->durakStateMachine.process_event (leaveGame{ user->accountName.value () });
      gameMachines.erase (gameMachine);
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::DurakLeaveGameError{ "Could not find a game for Account Name: " + user->accountName.value () }));
    }
}
auto constexpr ALLOWED_DIFFERENCE_FOR_RANKED_GAME_MATCHMAKING = size_t{ 100 };
bool
isInRatingrange (size_t userRating, size_t lobbyAverageRating)
{
  auto const difference = userRating > lobbyAverageRating ? userRating - lobbyAverageRating : lobbyAverageRating - userRating;
  return difference < ALLOWED_DIFFERENCE_FOR_RANKED_GAME_MATCHMAKING;
}

bool
checkRating (size_t userRating, std::vector<std::string> const &accountNames)
{
  return isInRatingrange (userRating, averageRating (accountNames));
}

bool
matchingLobby (std::string const &accountName, GameLobby const &gameLobby, GameLobby::LobbyType const &lobbyType)
{
  if (gameLobby.lobbyAdminType == lobbyType && gameLobby._users.size () < gameLobby.maxUserCount ())
    {
      if (lobbyType == GameLobby::LobbyType::MatchMakingSystemRanked)
        {
          soci::session sql (soci::sqlite3, databaseName);
          if (auto userInDatabase = confu_soci::findStruct<database::Account> (sql, "accountName", accountName))
            {
              return checkRating (userInDatabase->rating, gameLobby.accountNames ());
            }
        }
      else
        {
          return true;
        }
    }
  else
    {
      return false;
    }
  return false;
}

void
joinMatchMakingQueue (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies, boost::asio::io_context &io_context, GameLobby::LobbyType const &lobbyType)
{
  if (ranges::find_if (gameLobbies,
                       [accountName = user->accountName] (auto const &gameLobby) {
                         auto const &accountNames = gameLobby.accountNames ();
                         return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                       })
      == gameLobbies.end ())
    {
      if (auto gameLobbyToAddUser = ranges::find_if (gameLobbies, [lobbyType, accountName = user->accountName.value ()] (GameLobby const &gameLobby) { return matchingLobby (accountName, gameLobby, lobbyType); }); gameLobbyToAddUser != gameLobbies.end ())
        {
          if (auto error = gameLobbyToAddUser->tryToAddUser (user))
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbyError{ user->accountName.value (), error.value () }));
            }
          else
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinMatchMakingQueueSuccess{}));
              if (gameLobbyToAddUser->_users.size () == gameLobbyToAddUser->maxUserCount ())
                {
                  gameLobbyToAddUser->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::AskIfUserWantsToJoinGame{}));
                  gameLobbyToAddUser->startTimerToAcceptTheInvite (io_context, [gameLobbyToAddUser, &gameLobbies] () {
                    auto usersToRemove = std::vector<std::shared_ptr<User>>{};
                    ranges::copy_if (gameLobbyToAddUser->_users, ranges::back_inserter (usersToRemove), [usersWhichAccepted = gameLobbyToAddUser->readyUsers] (std::shared_ptr<User> const &user) {
                      return ranges::find_if (usersWhichAccepted,
                                              [user] (std::shared_ptr<User> const &userWhoAccepted) {
                                                //
                                                return user->accountName.value () == userWhoAccepted->accountName.value ();
                                              })
                             == usersWhichAccepted.end ();
                    });
                    for (auto const &userToRemove : usersToRemove)
                      {
                        userToRemove->msgQueue.push_back (objectToStringWithObjectName (shared_class::AskIfUserWantsToJoinGameTimeOut{}));
                        gameLobbyToAddUser->removeUser (userToRemove);
                      }
                    if (gameLobbyToAddUser->_users.empty ())
                      {
                        gameLobbies.erase (gameLobbyToAddUser);
                      }
                    else
                      {
                        gameLobbyToAddUser->sendToAllAccountsInGameLobby (objectToStringWithObjectName (shared_class::GameStartCanceled{}));
                      }
                  });
                }
            }
        }
      else
        {
          auto gameLobby = GameLobby{};
          gameLobby.lobbyAdminType = lobbyType;
          if (auto error = gameLobby.tryToAddUser (user))
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinGameLobbyError{ user->accountName.value (), error.value () }));
            }
          gameLobbies.emplace_back (gameLobby);
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinMatchMakingQueueSuccess{}));
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::JoinMatchMakingQueueError{ "User is allready in gamelobby" }));
    }
}

void
wantsToJoinGame (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies, std::list<GameMachine> &gameMachines, boost::asio::io_context &io_context)
{
  if (auto gameLobby = ranges::find_if (gameLobbies,
                                        [accountName = user->accountName] (auto const &gameLobby) {
                                          auto const &accountNames = gameLobby.accountNames ();
                                          return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                        });
      gameLobby != gameLobbies.end ())
    {
      if (stringToObject<shared_class::WantsToJoinGame> (objectAsString).answer)
        {
          if (ranges::find_if (gameLobby->readyUsers, [accountName = user->accountName.value ()] (std::shared_ptr<User> _user) { return _user->accountName == accountName; }) == gameLobby->readyUsers.end ())
            {
              gameLobby->readyUsers.push_back (user);
              if (gameLobby->readyUsers.size () == gameLobby->_users.size ())
                {
                  gameLobby->cancelTimer ();
                  ranges::for_each (gameLobby->_users, [] (auto &_user) { _user->msgQueue.push_back (objectToStringWithObjectName (shared_class::StartGame{})); });
                  auto names = std::vector<std::string>{};
                  ranges::transform (gameLobby->_users, ranges::back_inserter (names), [] (auto const &tempUser) { return tempUser->accountName.value (); });
                  auto game = durak::Game{ std::move (names), gameLobby->gameOption };
                  gameMachines.emplace_back (
                      game, gameLobby->_users, io_context, gameLobby->timerOption,
                      [accountName = user->accountName, &gameMachines] () {
                        if (auto gameWithUser = ranges::find_if (gameMachines,
                                                                 [accountName] (auto const &gameMachine) {
                                                                   auto accountNames = std::vector<std::string>{};
                                                                   ranges::transform (gameMachine.getGameUsers (), ranges::back_inserter (accountNames), [] (auto const &gameUser) { return gameUser._user->accountName.value (); });
                                                                   return ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                                                 });
                            gameWithUser != gameMachines.end ())
                          {
                            gameMachines.erase (gameWithUser);
                          }
                      },
                      gameLobby->lobbyAdminType);
                  gameLobbies.erase (gameLobby);
                }
            }
          else
            {
              user->msgQueue.push_back (objectToStringWithObjectName (shared_class::WantsToJoinGameError{ "You already accepted to join the game" }));
            }
        }
      else
        {
          user->msgQueue.push_back (objectToStringWithObjectName (shared_class::GameStartCanceledRemovedFromQueue{}));
          gameLobby->removeUser (user);
          gameLobby->cancelTimer ();
          if (gameLobby->_users.empty ())
            {
              gameLobbies.erase (gameLobby);
            }
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::WantsToJoinGameError{ "No game to join" }));
    }
}

void
leaveMatchMakingQueue (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies)
{
  if (auto gameLobby = ranges::find_if (gameLobbies,
                                        [accountName = user->accountName] (auto const &gameLobby) {
                                          auto const &accountNames = gameLobby.accountNames ();
                                          return gameLobby.lobbyAdminType != GameLobby::LobbyType::FirstUserInLobbyUsers && ranges::find_if (accountNames, [&accountName] (auto const &nameToCheck) { return nameToCheck == accountName; }) != accountNames.end ();
                                        });
      gameLobby != gameLobbies.end ())
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveQuickGameQueueSuccess{}));
      gameLobby->removeUser (user);
      gameLobby->cancelTimer ();
      if (gameLobby->_users.empty ())
        {
          gameLobbies.erase (gameLobby);
        }
    }
  else
    {
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LeaveQuickGameQueueError{ "User is not in queue" }));
    }
}

void
loginAsGuest (std::shared_ptr<User> user)
{
  if (not user->accountName)
    {
      user->accountName = to_string (boost::uuids::random_generator () ());
      user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LoginAsGuestSuccess{ user->accountName.value () }));
    }
}

void
removeUserFromLobbyAndGame (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies, std::list<GameMachine> &gameMachines)
{
  auto const findLobby = [userAccountName = user->accountName] (GameLobby const &gameLobby) {
    auto const &accountNamesToCheck = gameLobby.accountNames ();
    return ranges::find_if (accountNamesToCheck, [userAccountName] (std::string const &accountNameToCheck) { return userAccountName == accountNameToCheck; }) != accountNamesToCheck.end ();
  };
  auto gameLobbyWithUser = ranges::find_if (gameLobbies, findLobby);
  while (gameLobbyWithUser != gameLobbies.end ())
    {
      gameLobbyWithUser->removeUser (user);
      if (gameLobbyWithUser->_users.empty ())
        {
          gameLobbies.erase (gameLobbyWithUser);
        }
      gameLobbyWithUser = ranges::find_if (gameLobbies, findLobby);
    }
  auto const findGame = [userAccountName = user->accountName] (GameMachine const &gameMachine) {
    auto const &gameUsers = gameMachine.getGameUsers ();
    return ranges::find_if (gameUsers, [userAccountName] (GameUser const &gameUser) { return userAccountName == gameUser._user->accountName; }) != gameUsers.end ();
  };
  auto gameWithUser = ranges::find_if (gameMachines, findGame);
  while (gameWithUser != gameMachines.end ())
    {
      gameWithUser->durakStateMachine.process_event (leaveGame{ user->accountName.value () });
      gameMachines.erase (gameWithUser);
      gameWithUser = ranges::find_if (gameMachines, findGame);
    }
}

void
logoutAccount (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbies, std::list<GameMachine> &gameMachines)
{
  removeUserFromLobbyAndGame (user, gameLobbies, gameMachines);
  user->accountName = {};
  user->msgQueue.clear ();
  user->communicationChannels.clear ();
  user->ignoreLogin = false;
  user->ignoreCreateAccount = false;
  user->msgQueue.push_back (objectToStringWithObjectName (shared_class::LogoutAccountSuccess{}));
}