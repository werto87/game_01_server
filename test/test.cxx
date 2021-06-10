#include "src/database/database.hxx"
#include "src/logic/logic.hxx"
#include "src/pw_hash/passwordHash.hxx"
#include "src/server/user.hxx"
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/archive/basic_text_iarchive.hpp>
#include <boost/archive/basic_text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/fusion/include/pair.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/include/value_at.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/sequence/intrinsic/value_at.hpp>
#include <boost/fusion/support/pair.hpp>
#include <boost/json/src.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/type_index.hpp>
#include <catch2/catch.hpp>
#include <confu_json/confu_json.hxx>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <functional>
#include <game_01_shared_class/serialization.hxx>
#include <iostream>
#include <iterator>
#include <sodium.h>
#include <span>
#include <src/database/database.hxx>
/*
namespace test
{


TEST_CASE ("check_hashed_pw", "[check_hashed_pw]")
{
  if (sodium_init () < 0)
    {
      std::cout << "sodium_init => 0" << std::endl;
      std::terminate ();
      //panic! the library couldn't be initialized, it is not safe to use
}
auto pw = std::string{ "hello world" };
auto pw_hash = pw_to_hash (pw);
std::cout << pw_hash << std::endl;
REQUIRE (check_hashed_pw (pw_hash, pw));
}

TEST_CASE ("playground ", "[playground]")
{
  // playground
}
}
*/