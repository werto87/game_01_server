#include "src/server/server.hxx"
#include "src/logic/logic.hxx"
#include <algorithm>
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/range/adaptor/indexed.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/type_index.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <coroutine>
#include <iostream>
#include <sodium.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>

using namespace boost::beast;
using namespace boost::asio;

Server::Server (boost::asio::io_context &io_context, boost::asio::thread_pool &pool) : _io_context{ io_context }, _pool{ pool } {}

awaitable<std::string>
Server::my_read (websocket::stream<tcp_stream> &ws_)
{
  std::cout << "read" << std::endl;
  flat_buffer buffer;
  co_await ws_.async_read (buffer, use_awaitable);
  auto msg = buffers_to_string (buffer.data ());
  std::cout << "number of letters '" << msg.size () << "' msg: '" << msg << "'" << std::endl;
  co_return msg;
}

awaitable<void>
Server::readFromClient (size_t key)
{
  auto &user = users.at (key);
  try
    {
      for (;;)
        {
          auto readResult = co_await my_read (user.websocket);
          auto result = co_await handleMessage (readResult, _io_context, _pool, users, user);
          user.msgQueue.insert (user.msgQueue.end (), make_move_iterator (result.begin ()), make_move_iterator (result.end ()));
        }
    }
  catch (std::exception &e)
    {
      user.websocket.close ("lost connection to user");
      users.erase (key);
      std::cout << "echo  Exception: " << e.what () << std::endl;
    }
}

awaitable<void>
Server::writeToClient (size_t key)
{
  auto &user = users.at (key);
  try
    {
      for (;;)
        {
          auto timer = steady_timer (co_await this_coro::executor);
          using namespace std::chrono_literals;
          timer.expires_after (1s);
          co_await timer.async_wait (use_awaitable);
          while (not user.msgQueue.empty ())
            {
              auto tmpMsg = std::move (user.msgQueue.front ());
              std::cout << "send msg: " << tmpMsg << std::endl;
              user.msgQueue.pop_front ();
              co_await user.websocket.async_write (buffer (tmpMsg), use_awaitable);
            }
        }
    }
  catch (std::exception &e)
    {
      user.websocket.close ("lost connection to user");
      users.erase (key);
      std::printf ("echo Exception:  %s\n", e.what ());
    }
}

awaitable<void>
Server::listener ()
{
  auto executor = co_await this_coro::executor;
  ip::tcp::acceptor acceptor (executor, { ip::tcp::v4 (), 55555 });
  for (;;)
    {
      ip::tcp::socket socket = co_await acceptor.async_accept (use_awaitable);
      auto key = size_t{};
      randombytes_buf (&key, sizeof (key));
      users.emplace (key, User{ {}, boost::beast::websocket::stream<boost::beast::tcp_stream>{ std::move (socket) }, {}, { "default" } });
      users.at (key).websocket.set_option (websocket::stream_base::timeout::suggested (role_type::server));
      users.at (key).websocket.set_option (websocket::stream_base::decorator ([] (websocket::response_type &res) { res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-server-async"); }));
      co_await users.at (key).websocket.async_accept (use_awaitable);
      co_spawn (
          executor, [this, key] () mutable { return readFromClient (key); }, detached);
      co_spawn (
          executor, [this, key] () mutable { return writeToClient (key); }, detached);
    }
}
