#include "src/database/database.hxx"
#include "src/database/constant.hxx"
#include <boost/lexical_cast.hpp>
#include <boost/optional/optional.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <filesystem>

namespace database
{

void
createEmptyDatabase ()
{
  std::filesystem::create_directory (std::filesystem::path{ pathToTestDatabase }.parent_path ());
  std::filesystem::copy_file (pathToTemplateDatabase, pathToTestDatabase, std::filesystem::copy_options::overwrite_existing);
}

void
createTables ()
{
  soci::session sql (soci::sqlite3, pathToTestDatabase);
  confu_soci::createTableForStruct<Account> (sql);
}

boost::optional<Account>
createAccount (std::string const &accountName, std::string const &password)
{
  soci::session sql (soci::sqlite3, pathToTestDatabase);
  return confu_soci::findStruct<Account> (sql, "accountName", confu_soci::insertStruct (sql, Account{ accountName, password }, true));
}

} // namespace database