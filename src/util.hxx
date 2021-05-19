#ifndef EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1
#define EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1

#include "src/server/user.hxx"
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <durak/game.hxx>
#include <durak/gameData.hxx>

template <typename TypeToSend>
std::string
objectToStringWithObjectName (TypeToSend const &typeToSend)
{
  return confu_soci::typeNameWithOutNamespace (typeToSend) + '|' + confu_boost::toString (typeToSend);
}

durak::GameData filterGameDataByAccountName (durak::GameData const &gameData, std::string const &accountName);

void sendGameDataToAccountsInGame (durak::Game const &game, std::vector<std::shared_ptr<User>> &users);

#endif /* EBD66723_6B6F_4460_A3DE_00AEB1E6D6B1 */
