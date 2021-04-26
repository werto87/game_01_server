#include "src/database/database.hxx"
#include "src/logic/logic.hxx"
#include "src/pw_hash/passwordHash.hxx"
#include "src/server/user.hxx"
#include <boost/archive/basic_text_iarchive.hpp>
#include <boost/archive/basic_text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <catch2/catch.hpp>
#include <confu_soci/convenienceFunctionForSoci.hxx>
#include <game_01_shared_class/serialization.hxx>
#include <iostream>
#include <iterator>
#include <sodium.h>
#include <span>
namespace test
{

TEST_CASE ("check_hashed_pw", "[check_hashed_pw]")
{
  if (sodium_init () < 0)
    {
      std::cout << "sodium_init => 0" << std::endl;
      std::terminate ();
      /* panic! the library couldn't be initialized, it is not safe to use */
    }
  auto pw = std::string{ "hello world" };
  auto pw_hash = pw_to_hash (pw);
  std::cout << pw_hash << std::endl;
  REQUIRE (check_hashed_pw (pw_hash, pw));
}

TEST_CASE ("joinChannelasda ", "[joinChannelasdsa]")
{
  boost::asio::io_context io_context (1);
  boost::asio::ip::tcp::socket socket{ io_context };
  auto user = User{ {}, boost::beast::websocket::stream<boost::beast::tcp_stream>{ std::move (socket) }, {}, { "default" } };
  auto result = joinChannel (shared_class::JoinChannel{ .channel = { "testChannel" } }, user);
  std::cout << confu_boost::toString (shared_class::JoinChannel{ .channel = { "testChannel" } }) << std::endl;
  REQUIRE (result);
  std::cout << result.value ();
}
}