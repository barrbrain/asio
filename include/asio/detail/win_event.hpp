//
// win_event.hpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2003-2005 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_WIN_EVENT_HPP
#define ASIO_DETAIL_WIN_EVENT_HPP

#include "asio/detail/push_options.hpp"

#if defined(_WIN32)

#include "asio/detail/push_options.hpp"
#include <new>
#include "asio/detail/pop_options.hpp"

#include "asio/detail/socket_types.hpp"

namespace asio {
namespace detail {

class win_event
  : private boost::noncopyable
{
public:
  // Constructor.
  win_event()
    : event_(::CreateEvent(0, true, false, 0))
  {
    if (!event_)
      throw std::bad_alloc();
  }

  // Destructor.
  ~win_event()
  {
    ::CloseHandle(event_);
  }

  // Signal the event.
  void signal()
  {
    ::SetEvent(event_);
  }

  // Reset the event.
  void clear()
  {
    ::ResetEvent(event_);
  }

  // Wait for the event to become signalled.
  void wait()
  {
    ::WaitForSingleObject(event_, INFINITE);
  }

private:
  HANDLE event_;
};

} // namespace detail
} // namespace asio

#endif // defined(_WIN32)

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_WIN_EVENT_HPP