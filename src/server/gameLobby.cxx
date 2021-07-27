#include "src/server/gameLobby.hxx"
#include <algorithm>
#include <iterator>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/count_if.hpp>
#include <stdexcept>
std::optional<std::string>
GameLobby::setMaxUserCount (size_t userMaxCount)
{
  if (userMaxCount < 1)
    {
      return "userMaxCount < 1";
    }
  else
    {
      if (userMaxCount < _users.size ())
        {
          return "userMaxCount < _users.size ()";
        }
      else
        {
          _maxUserCount = userMaxCount;
        }
    }
  return {};
}

std::vector<std::string>
GameLobby::accountNames () const
{
  auto result = std::vector<std::string>{};
  ranges::transform (_users, ranges::back_inserter (result), [] (auto const &user) { return user->accountName.value_or ("Error User is not Logged  in but still in GameLobby"); });
  return result;
}

std::string
GameLobby::gameLobbyAdminAccountName () const
{
  return _users.front ()->accountName.value ();
}

size_t
GameLobby::maxUserCount () const
{
  return _maxUserCount;
}

std::string const &
GameLobby::gameLobbyPassword () const
{
  return _password;
}

std::string const &
GameLobby::gameLobbyName () const
{
  return _name;
}

std::optional<std::string>
GameLobby::tryToAddUser (std::shared_ptr<User> const &user)
{
  if (_maxUserCount > _users.size ())
    {
      if (ranges::none_of (_users, [accountName = user->accountName.value ()] (std::shared_ptr<User> const &user) { return user->accountName == accountName; }))
        {
          _users.push_back (user);
          return {};
        }
      else
        {
          return "User allready in lobby with user name: " + user->accountName.value ();
        }
    }
  else
    {
      return "Lobby full";
    }
}

bool
GameLobby::tryToRemoveUser (std::string const &userWhoTriesToRemove, std::string const &userToRemoveName)
{
  if (userWhoTriesToRemove == gameLobbyAdminAccountName () && userWhoTriesToRemove != userToRemoveName)
    {
      if (auto userToRemoveFromLobby = ranges::find_if (_users, [&userToRemoveName] (auto const &user) { return userToRemoveName == user->accountName; }); userToRemoveFromLobby != _users.end ())
        {
          _users.erase (userToRemoveFromLobby);
          return true;
        }
    }
  return false;
}

bool
GameLobby::tryToRemoveAdminAndSetNewAdmin ()
{
  if (not _users.empty ())
    {
      _users.erase (_users.begin ());
      return true;
    }
  else
    {
      return false;
    }
}

bool
GameLobby::hasPassword () const
{
  return not _password.empty ();
}

void
GameLobby::sendToAllAccountsInGameLobby (std::string const &message)
{
  // TODO do not push messages in a queue with offline user
  // if user is offline but in lobby user message queue gets filled but the messages get never send because of GameLobby::relogUser() overrides user with another user before the messages gets send. This is fine but we still push messages in a queue and override it later
  ranges::for_each (_users, [&message] (auto &user) { user->msgQueue.push_back (message); });
}

bool
GameLobby::removeUser (std::shared_ptr<User> const &user)
{
  _users.erase (std::remove_if (_users.begin (), _users.end (), [accountName = user->accountName.value ()] (auto const &_user) { return accountName == _user->accountName.value (); }), _users.end ());
  return _users.empty ();
}

size_t
GameLobby::accountCount ()
{
  return _users.size ();
}

void
GameLobby::relogUser (std::shared_ptr<User> &user)
{
  if (auto oldLogin = ranges::find_if (_users, [accountName = user->accountName.value ()] (auto const &_user) { return accountName == _user->accountName.value (); }); oldLogin != _users.end ())
    {
      *oldLogin = user;
    }
  else
    {
      throw std::logic_error{ "can not relog user beacuse he is not logged in the create game lobby" };
    }
}
