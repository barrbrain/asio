//
// buffer_resize_guard.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2005 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_BUFFER_RESIZE_GUARD_HPP
#define ASIO_DETAIL_BUFFER_RESIZE_GUARD_HPP

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

// Helper class to manage buffer resizing in an exception safe way.
template <typename Buffer>
class buffer_resize_guard
{
public:
  // Constructor.
  buffer_resize_guard(Buffer& buffer)
    : buffer_(buffer),
      old_size_(buffer.size()),
      committed_(false)
  {
  }

  // Destructor rolls back the buffer resize unless commit was called.
  ~buffer_resize_guard()
  {
    if (!committed_)
      buffer_.resize(old_size_);
  }

  // Commit the resize transaction.
  void commit()
  {
    committed_ = true;
  }

private:
  // The buffer being managed.
  Buffer& buffer_;

  // The size of the buffer at the time the guard was constructed.
  size_t old_size_;

  // Whether commit has been called.
  bool committed_;
};

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_BUFFER_RESIZE_GUARD_HPP