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
#include <cstddef>
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
Server::readFromClient (std::list<std::shared_ptr<User> >::iterator user)
{
  try
    {
      for (;;)
        {
          // workaround for internal compiler error with shared pointer and co_await
          // BEGIN---------------------------------------------------------------------
          auto tempUser = user->get ();
          auto readResult = co_await my_read (tempUser->websocket);
          auto result = co_await handleMessage (readResult, _io_context, _pool, users, *tempUser);
          tempUser->msgQueue.insert (tempUser->msgQueue.end (), make_move_iterator (result.begin ()), make_move_iterator (result.end ()));
          // END-----------------------------------------------------------------------
          // comment this in when compiler error got fixed
          // BEGIN---------------------------------------------------------------------
          // auto readResult = co_await my_read (user->get ()->websocket);
          // auto result = co_await handleMessage (readResult, _io_context, _pool, users, *user->get ());
          // user->get ()->msgQueue.insert (user->get ()->msgQueue.end (), make_move_iterator (result.begin ()), make_move_iterator (result.end ()));
          // END-----------------------------------------------------------------------
        }
    }
  catch (std::exception &e)
    {
      std::cout << "echo  Exception: " << e.what () << std::endl;
      removeUser (user);
    }
}

void
Server::removeUser (std::list<std::shared_ptr<User> >::iterator user)
{
  try
    {
      user->get ()->websocket.close ("lost connection to user");
    }
  catch (std::exception &e)
    {
      std::cout << "echo  Exception: " << e.what () << std::endl;
    }
  users.erase (user);
}

awaitable<void>
Server::writeToClient (std::list<std::shared_ptr<User> >::iterator user)
{
  try
    {
      for (;;)
        {
          auto timer = steady_timer (co_await this_coro::executor);
          using namespace std::chrono_literals;
          timer.expires_after (1s);
          co_await timer.async_wait (use_awaitable);
          while (not user->get ()->msgQueue.empty ())
            {
              auto tmpMsg = std::move (user->get ()->msgQueue.front ());
              std::cout << "send msg: " << tmpMsg << std::endl;
              user->get ()->msgQueue.pop_front ();
              // workaround for internal compiler error with shared pointer and co_await
              // BEGIN---------------------------------------------------------------------
              auto tempUser = user->get ();
              co_await tempUser->websocket.async_write (buffer (tmpMsg), use_awaitable);
              // END-----------------------------------------------------------------------
              // comment this in when compiler error got fixed
              // BEGIN---------------------------------------------------------------------
              // co_await user->get ()->websocket.async_write (buffer (tmpMsg), use_awaitable);
              // END-----------------------------------------------------------------------
            }
        }
    }
  catch (std::exception &e)
    {
      std::cout << "echo  Exception: " << e.what () << std::endl;
      removeUser (user);
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
      users.emplace_back (std::make_shared<User> (User{ {}, boost::beast::websocket::stream<boost::beast::tcp_stream>{ std::move (socket) }, {}, { "default" } }));
      std::list<std::shared_ptr<User> >::iterator user = std::next (users.end (), -1);
      user->get ()->websocket.set_option (websocket::stream_base::timeout::suggested (role_type::server));
      user->get ()->websocket.set_option (websocket::stream_base::decorator ([] (websocket::response_type &res) { res.set (http::field::server, std::string (BOOST_BEAST_VERSION_STRING) + " websocket-server-async"); }));
      // workaround for internal compiler error with shared pointer and co_await
      // BEGIN---------------------------------------------------------------------
      auto tempUser = user->get ();
      co_await tempUser->websocket.async_accept (use_awaitable);
      // END-----------------------------------------------------------------------
      // comment this in when compiler error got fixed
      // BEGIN---------------------------------------------------------------------
      // co_await user->get ()->websocket.async_accept (use_awaitable);
      // END-----------------------------------------------------------------------
      co_spawn (
          executor, [&] () mutable { return readFromClient (user); }, detached);
      co_spawn (
          executor, [&] () mutable { return writeToClient (user); }, detached);
    }
}
