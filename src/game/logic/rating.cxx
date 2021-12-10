#include "src/database/database.hxx"
#include <boost/numeric/conversion/cast.hpp>
#include <cstddef>
#include <pipes/pipes.hpp>
#include <range/v3/numeric/accumulate.hpp>

long
ratingChange (size_t userRating, size_t otherUserRating, long double score, size_t ratingChangeFactor)
{
  auto const expectedScore = 1 / (1 + std::pow (10, (boost::numeric_cast<long double> (otherUserRating) - boost::numeric_cast<long double> (userRating)) / 400));
  return std::lrintl (ratingChangeFactor * (score - expectedScore));
}

size_t
averageRating (std::vector<size_t> const &ratings)
{
  auto accountsRatingSum = 0;
  ratings >>= pipes::for_each ([&accountsRatingSum] (auto rating) { accountsRatingSum += rating; });
  return boost::numeric_cast<size_t> (std::rintl (boost::numeric_cast<long double> (accountsRatingSum) / ratings.size ()));
}

size_t
averageRating (size_t sum, size_t elements)
{
  return boost::numeric_cast<size_t> (std::rintl (boost::numeric_cast<long double> (sum) / elements));
}

// In the winning team the user with the most rating gets the least points
size_t
ratingShareWinningTeam (size_t userRating, std::vector<size_t> const &userRatings, size_t ratingChange)
{
  auto const inverseSum = ranges::accumulate (userRatings, boost::numeric_cast<long double> (0), [] (long double sum, long double value) { return sum + (boost::numeric_cast<long double> (1) / value); });
  return boost::numeric_cast<size_t> (std::rintl ((boost::numeric_cast<long double> (1) / (boost::numeric_cast<long double> (userRating)) / inverseSum) * ratingChange));
}

// In the losing team the user with the most rating loses the most points
size_t
ratingShareLosingTeam (size_t userRating, std::vector<size_t> const &userRatings, size_t ratingChange)
{
  auto const sum = ranges::accumulate (userRatings, boost::numeric_cast<long double> (0), [] (size_t sum, size_t value) { return sum + value; });
  return boost::numeric_cast<size_t> (std::rintl ((userRating / sum) * ratingChange));
}
