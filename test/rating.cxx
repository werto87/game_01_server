#include "src/game/logic/rating.hxx"
#include "test/util.hxx"
#include <boost/numeric/conversion/cast.hpp>
#include <catch2/catch.hpp>
namespace test
{

TEST_CASE ("averageRating", "[rating]") { CHECK (averageRating ({ 100, 34, 53, 2342, 452 }) == 596); }
TEST_CASE ("ratingChangeLose", "[rating]") { CHECK (ratingChange (1987, 2321, 0, 20) == -3); }
TEST_CASE ("ratingChangeWin", "[rating]") { CHECK (ratingChange (1987, 2321, 1, 20) == 17); }
TEST_CASE ("ratingShareWinningTeam", "[rating]") { CHECK (ratingShareWinningTeam (1000, { 1000, 2000 }, 20) + ratingShareWinningTeam (2000, { 1000, 2000 }, 20) == 20); }
TEST_CASE ("ratingShareLosingTeam", "[rating]") { CHECK (ratingShareLosingTeam (1000, { 1000, 2000 }, 20) + ratingShareLosingTeam (2000, { 1000, 2000 }, 20) == 20); }

}