#include "src/pw_hash/passwordHash.hxx"
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
#include <thread>
#include <utility>

std::string
pw_to_hash (std::string const &password)
{
  auto hashed_password = std::array<char, crypto_pwhash_STRBYTES>{};
  if (crypto_pwhash_str (hashed_password.begin (), password.data (), password.size (), 3, crypto_pwhash_MEMLIMIT_SENSITIVE / 64) != 0)
    {
      std::cout << "out of memory" << std::endl;
      std::terminate ();
    }
  auto result = std::string{};
  std::copy_if (hashed_password.begin (), hashed_password.end (), std::back_inserter (result), [] (auto value) { return value != 0; });
  return result;
}

bool
check_hashed_pw (std::string const &hashedPassword, std::string const &password)
{
  return crypto_pwhash_str_verify (hashedPassword.data (), password.data (), password.size ()) == 0;
}
