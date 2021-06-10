#ifndef B86FE02F_B7D0_4435_9031_A334C305B294
#define B86FE02F_B7D0_4435_9031_A334C305B294

#include "confu_soci/convenienceFunctionForSoci.hxx"
#include "src/database/constant.hxx"
#include <boost/optional.hpp>
#include <filesystem>

BOOST_FUSION_DEFINE_STRUCT ((database), Account, (std::string, accountName) (std::string, password))

namespace database
{
void createEmptyDatabase ();
void createTables ();

boost::optional<database::Account> createAccount (std::string const &accountName, std::string const &password);
}

#endif /* B86FE02F_B7D0_4435_9031_A334C305B294 */
