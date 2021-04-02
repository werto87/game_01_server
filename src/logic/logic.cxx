#include "src/logic/logic.hxx"
#include "src/database/database.hxx"
#include <algorithm>
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/type_index.hpp>
#include <crypt.h>
#include <sodium.h>
#include <sstream>
#include <string>

std::vector<std::string>
handleMessage (std::string const &msg)
{
  auto result = std::vector<std::string>{};
  if (boost::algorithm::starts_with (msg, "create account|"))
    {
      if (auto accountAsString = createAccount (msg))
        {
          result.push_back (accountAsString.value ());
        }
    }
  else if (boost::algorithm::contains (msg, "create character|"))
    {
      if (auto characterAsString = createCharacter (msg))
        {
          result.push_back (characterAsString.value ());
        }
    }
  return result;
}

boost::optional<std::string>
createAccount (std::string const &msg)
{
  auto accountStringStream = std::stringstream{};
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () >= 2)
    {
      boost::algorithm::split (splitMesssage, splitMesssage.at (1), boost::is_any_of (","));
      if (splitMesssage.size () >= 2)
        {
          // hash pw

          if (auto account = database::createAccount (splitMesssage.at (0),
                                                      // hash this plx
                                                      splitMesssage.at (1)
                                                      // hash this plx
                                                      ))
            {
              boost::archive::text_oarchive accountArchive{ accountStringStream };
              accountArchive << account.value ();
            }
        }
    }
  return accountStringStream.str ();
}

boost::optional<std::string>
createCharacter (std::string const &msg)
{
  auto characterStringStream = std::stringstream{};
  std::vector<std::string> splitMesssage{};
  boost::algorithm::split (splitMesssage, msg, boost::is_any_of ("|"));
  if (splitMesssage.size () >= 2)
    {
      if (auto character = database::createCharacter (splitMesssage.at (1)))
        {
          boost::archive::text_oarchive characterArchive{ characterStringStream };
          characterArchive << character.value ();
        }
    }
  return characterStringStream.str ();
}