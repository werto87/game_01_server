#ifndef AC74F6B0_DAFF_472A_961E_2BCFAB96FB14
#define AC74F6B0_DAFF_472A_961E_2BCFAB96FB14

#include "src/database/database.hxx"
#include <cstddef>
#include <vector>

size_t ratingChange (size_t userRating, size_t otherUserRating, long double score, size_t ratingChangeFactor);

size_t averageRating (std::vector<size_t> const &ratings);

size_t ratingShareWinningTeam (size_t userRating, std::vector<size_t> const &userRatings, size_t ratingChange);

size_t ratingShareLosingTeam (size_t userRating, std::vector<size_t> const &userRatings, size_t ratingChange);

#endif /* AC74F6B0_DAFF_472A_961E_2BCFAB96FB14 */
