//
// select_interrupter.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2005 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_SELECT_INTERRUPTER_HPP
#define ASIO_DETAIL_SELECT_INTERRUPTER_HPP

#include "asio/detail/push_options.hpp"

#include "asio/detail/pipe_select_interrupter.hpp"
#include "asio/detail/socket_select_interrupter.hpp"

namespace asio {
namespace detail {

#if defined(_WIN32)
typedef socket_select_interrupter select_interrupter;
#else
typedef pipe_select_interrupter select_interrupter;
#endif

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_DETAIL_SELECT_INTERRUPTER_HPP