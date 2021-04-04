#ifndef AD3B06C2_4AC7_438D_8907_4643053A4E7E
#define AD3B06C2_4AC7_438D_8907_4643053A4E7E
#include <algorithm>
#include <array>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/saved_handler.hpp>
#include <chrono>
#include <coroutine>
#include <cstddef>
#include <iostream>
#include <memory>
#include <sodium.h>
#include <string>
#include <thread>
#include <utility>

std::string pw_to_hash (std::string const &password);

bool check_hashed_pw (std::string const &hashedPassword, std::string const &password);

template <boost::asio::completion_token_for<void (std::string)> CompletionToken>
auto
async_hash (boost::asio::thread_pool &pool, boost::asio::io_context &io_context, std::string const &password, CompletionToken &&token)
{
  return boost::asio::async_initiate<CompletionToken, void (std::string)> (
      [&] (auto completion_handler, std::string const &passwordToHash) {
        boost::asio::post (pool, [&, completion_handler = std::move (completion_handler), passwordToHash] () mutable {
          auto hashedPw = pw_to_hash (passwordToHash);
          boost::asio::post (io_context, [hashedPw = std::move (hashedPw), completion_handler = std::move (completion_handler)] () mutable { completion_handler (hashedPw); });
        });
      },
      token, password);
}

template <boost::asio::completion_token_for<void (bool)> CompletionToken>
auto
async_check_hashed_pw (boost::asio::thread_pool &pool, boost::asio::io_context &io_context, std::string const &password, std::string const &hashedPassword, CompletionToken &&token)
{
  return boost::asio::async_initiate<CompletionToken, void (bool)> (
      [&] (auto completion_handler, std::string const &passwordToCheck, std::string const &hashedPw) {
        boost::asio::post (pool, [&, completion_handler = std::move (completion_handler), passwordToCheck, hashedPw] () mutable {
          auto isCorrectPw = check_hashed_pw (passwordToCheck, hashedPw);
          boost::asio::post (io_context, [isCorrectPw, completion_handler = std::move (completion_handler)] () mutable { completion_handler (isCorrectPw); });
        });
      },
      token, password, hashedPassword);
}

#endif /* AD3B06C2_4AC7_438D_8907_4643053A4E7E */
