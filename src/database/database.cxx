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
  std::filesystem::remove (databaseName.c_str ());
  if (not std::filesystem::exists ("database"))
    {
      std::filesystem::create_directory ("database");
    }
  using namespace sqlite_api;
  sqlite3 *db;
  int rc;
  rc = sqlite3_open (databaseName.c_str (), &db);
  if (rc)
    {
      fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
      return;
    }
  sqlite3_close (db);
}

void
createDatabaseIfNotExist ()
{
  using namespace sqlite_api;
  if (not std::filesystem::exists ("database"))
    {
      std::filesystem::create_directory ("database");
    }
  sqlite3 *db;
  int rc;
  rc = sqlite3_open (databaseName.c_str (), &db);
  if (rc)
    {
      fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg (db));
      return;
    }
  sqlite3_close (db);
}

void
createTables ()
{
  soci::session sql (soci::sqlite3, databaseName);
  try
    {
      confu_soci::createTableForStruct<Account> (sql);
    }
  catch (soci::soci_error const &error)
    {
      std::cout << error.get_error_message () << std::endl;
    }
}

auto constexpr START_RATING = 1500;

boost::optional<Account>
createAccount (std::string const &accountName, std::string const &password)
{
  soci::session sql (soci::sqlite3, databaseName);
  return confu_soci::findStruct<Account> (sql, "accountName", confu_soci::insertStruct (sql, Account{ accountName, password, START_RATING }, true));
}

} // namespace database