#ifndef EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1
#define EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1
#include "confu_json/confu_json.hxx"
#include "src/game/gameUser.hxx"
#include "src/game/logic/allowedMoves.hxx"
#include "src/server/user.hxx"
#include <durak/game.hxx>
#include <durak/gameData.hxx>
template <typename TypeToSend>
std::string
objectToStringWithObjectName (TypeToSend const &typeToSend)
{
  std::stringstream ss{};
  ss << confu_json::type_name<TypeToSend> () << '|' << confu_json::to_json (typeToSend);
  return ss.str ();
}

template <typename T>
T
stringToObject (std::string const &objectAsString)
{
  T t{};
  boost::json::error_code ec{};
  auto jsonValue = confu_json::read_json (objectAsString, ec);
  if (ec)
    {
      std::cerr << "error while parsing string: error code: " << ec << std::endl;
      std::cerr << "error while parsing string: stringToParse: " << objectAsString << std::endl;
    }
  else
    {
      t = confu_json::to_object<T> (jsonValue);
    }
  return t;
}

durak::GameData filterGameDataByAccountName (durak::GameData const &gameData, std::string const &accountName);

// allowed moves overrides
void sendAvailableMoves (durak::Game const &game, std::vector<GameUser> const &_gameUsers, AllowedMoves const &removeFromAllowedMoves = {}, AllowedMoves const &addToAllowedMoves = {});

void sendGameDataToAccountsInGame (durak::Game const &game, std::vector<GameUser> const &_gameUsers, AllowedMoves const &removeFromAllowedMoves = {}, AllowedMoves const &addToAllowedMoves = {});

#endif /* EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1 */
