#ifndef E18680A5_3B06_4019_A849_6CDB82D14796
#define E18680A5_3B06_4019_A849_6CDB82D14796
#include "src/game/logic/gameMachine.hxx"
#include "src/server/gameLobby.hxx"
#include "src/server/user.hxx"
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <game_01_shared_class/serialization.hxx>
#include <string>
#include <vector>

boost::asio::awaitable<void> handleMessage (std::string const &msg, boost::asio::io_context &io_context, boost::asio::thread_pool &pool, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines);
boost::asio::awaitable<void> createAccountAndLogin (std::string objectAsString, boost::asio::io_context &io_context, std::shared_ptr<User> user, boost::asio::thread_pool &pool);
boost::asio::awaitable<void> loginAccount (std::string objectAsString, boost::asio::io_context &io_context, std::list<std::shared_ptr<User>> &users, std::shared_ptr<User> user, boost::asio::thread_pool &pool, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines);
void logoutAccount (User &user);
void broadCastMessage (std::string const &objectAsString, std::list<std::shared_ptr<User>> &users, User &sendingUser);
void joinChannel (std::string const &objectAsString, User &user);
void leaveChannel (std::string const &objectAsString, User &user);
void createGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);
void joinGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);
void setMaxUserSizeInCreateGameLobby (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);
void setGameOption (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);
void leaveGameLobby (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);
void relogTo (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines);
void createGame (std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys, std::list<GameMachine> &gameMachines, boost::asio::io_context &io_context);
void durakAttack (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines);
void durakDefend (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines);
void durakDefendPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines);
void durakAttackPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines);
void durakAssistPass (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines);
void durakAskDefendWantToTakeCardsAnswer (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameMachine> &gameMachines);
void durakLeaveGame (std::shared_ptr<User> user, std::list<GameMachine> &gameMachines);
void loginAccountCancel (std::shared_ptr<User> user);
void createAccountCancel (std::shared_ptr<User> user);
void setTimerOption (std::string const &objectAsString, std::shared_ptr<User> user, std::list<GameLobby> &gameLobbys);

#endif /* E18680A5_3B06_4019_A849_6CDB82D14796 */
