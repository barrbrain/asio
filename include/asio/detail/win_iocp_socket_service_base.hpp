//
// detail/win_iocp_socket_service_base.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_DETAIL_WIN_IOCP_SOCKET_SERVICE_BASE_HPP
#define ASIO_DETAIL_WIN_IOCP_SOCKET_SERVICE_BASE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"

#if defined(ASIO_HAS_IOCP)

#include <boost/type_traits/is_same.hpp>
#include <boost/utility/addressof.hpp>
#include "asio/error.hpp"
#include "asio/io_service.hpp"
#include "asio/socket_base.hpp"
#include "asio/detail/bind_handler.hpp"
#include "asio/detail/buffer_sequence_adapter.hpp"
#include "asio/detail/fenced_block.hpp"
#include "asio/detail/handler_alloc_helpers.hpp"
#include "asio/detail/handler_invoke_helpers.hpp"
#include "asio/detail/mutex.hpp"
#include "asio/detail/operation.hpp"
#include "asio/detail/reactor.hpp"
#include "asio/detail/reactor_op.hpp"
#include "asio/detail/socket_holder.hpp"
#include "asio/detail/socket_ops.hpp"
#include "asio/detail/socket_types.hpp"
#include "asio/detail/win_iocp_io_service.hpp"
#include "asio/detail/win_iocp_null_buffers_op.hpp"
#include "asio/detail/win_iocp_socket_send_op.hpp"
#include "asio/detail/win_iocp_socket_recv_op.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {
namespace detail {

class win_iocp_socket_service_base
{
public:
  // The implementation type of the socket.
  struct base_implementation_type
  {
    // The native socket representation.
    socket_type socket_;

    // The current state of the socket.
    socket_ops::state_type state_;

    // We use a shared pointer as a cancellation token here to work around the
    // broken Windows support for cancellation. MSDN says that when you call
    // closesocket any outstanding WSARecv or WSASend operations will complete
    // with the error ERROR_OPERATION_ABORTED. In practice they complete with
    // ERROR_NETNAME_DELETED, which means you can't tell the difference between
    // a local cancellation and the socket being hard-closed by the peer.
    socket_ops::shared_cancel_token_type cancel_token_;

    // Per-descriptor data used by the reactor.
    reactor::per_descriptor_data reactor_data_;

#if defined(ASIO_ENABLE_CANCELIO)
    // The ID of the thread from which it is safe to cancel asynchronous
    // operations. 0 means no asynchronous operations have been started yet.
    // ~0 means asynchronous operations have been started from more than one
    // thread, and cancellation is not supported for the socket.
    DWORD safe_cancellation_thread_id_;
#endif // defined(ASIO_ENABLE_CANCELIO)

    // Pointers to adjacent socket implementations in linked list.
    base_implementation_type* next_;
    base_implementation_type* prev_;
  };

  // Constructor.
  ASIO_DECL win_iocp_socket_service_base(
      asio::io_service& io_service);

  // Destroy all user-defined handler objects owned by the service.
  ASIO_DECL void shutdown_service();

  // Construct a new socket implementation.
  ASIO_DECL void construct(base_implementation_type& impl);

  // Destroy a socket implementation.
  ASIO_DECL void destroy(base_implementation_type& impl);

  // Determine whether the socket is open.
  bool is_open(const base_implementation_type& impl) const
  {
    return impl.socket_ != invalid_socket;
  }

  // Destroy a socket implementation.
  ASIO_DECL asio::error_code close(
      base_implementation_type& impl, asio::error_code& ec);

  // Cancel all operations associated with the socket.
  ASIO_DECL asio::error_code cancel(
      base_implementation_type& impl, asio::error_code& ec);

  // Determine whether the socket is at the out-of-band data mark.
  bool at_mark(const base_implementation_type& impl,
      asio::error_code& ec) const
  {
    return socket_ops::sockatmark(impl.socket_, ec);
  }

  // Determine the number of bytes available for reading.
  std::size_t available(const base_implementation_type& impl,
      asio::error_code& ec) const
  {
    return socket_ops::available(impl.socket_, ec);
  }

  // Place the socket into the state where it will listen for new connections.
  asio::error_code listen(base_implementation_type& impl,
      int backlog, asio::error_code& ec)
  {
    socket_ops::listen(impl.socket_, backlog, ec);
    return ec;
  }

  // Perform an IO control command on the socket.
  template <typename IO_Control_Command>
  asio::error_code io_control(base_implementation_type& impl,
      IO_Control_Command& command, asio::error_code& ec)
  {
    socket_ops::ioctl(impl.socket_, impl.state_, command.name(),
        static_cast<ioctl_arg_type*>(command.data()), ec);
    return ec;
  }

  /// Disable sends or receives on the socket.
  asio::error_code shutdown(base_implementation_type& impl,
      socket_base::shutdown_type what, asio::error_code& ec)
  {
    socket_ops::shutdown(impl.socket_, what, ec);
    return ec;
  }

  // Send the given data to the peer. Returns the number of bytes sent.
  template <typename ConstBufferSequence>
  size_t send(base_implementation_type& impl,
      const ConstBufferSequence& buffers,
      socket_base::message_flags flags, asio::error_code& ec)
  {
    buffer_sequence_adapter<asio::const_buffer,
        ConstBufferSequence> bufs(buffers);

    return socket_ops::sync_send(impl.socket_, impl.state_,
        bufs.buffers(), bufs.count(), flags, bufs.all_empty(), ec);
  }

  // Wait until data can be sent without blocking.
  size_t send(base_implementation_type& impl, const null_buffers&,
      socket_base::message_flags, asio::error_code& ec)
  {
    // Wait for socket to become ready.
    socket_ops::poll_write(impl.socket_, ec);

    return 0;
  }

  // Start an asynchronous send. The data being sent must be valid for the
  // lifetime of the asynchronous operation.
  template <typename ConstBufferSequence, typename Handler>
  void async_send(base_implementation_type& impl,
      const ConstBufferSequence& buffers,
      socket_base::message_flags flags, Handler handler)
  {
    // Allocate and construct an operation to wrap the handler.
    typedef win_iocp_socket_send_op<ConstBufferSequence, Handler> op;
    typename op::ptr p = { boost::addressof(handler),
      asio_handler_alloc_helpers::allocate(
        sizeof(op), handler), 0 };
    p.p = new (p.v) op(impl.cancel_token_, buffers, handler);

    buffer_sequence_adapter<asio::const_buffer,
        ConstBufferSequence> bufs(buffers);

    start_send_op(impl, bufs.buffers(), bufs.count(), flags,
        (impl.state_ & socket_ops::stream_oriented) != 0 && bufs.all_empty(),
        p.p);
    p.v = p.p = 0;
  }

  // Start an asynchronous wait until data can be sent without blocking.
  template <typename Handler>
  void async_send(base_implementation_type& impl, const null_buffers&,
      socket_base::message_flags, Handler handler)
  {
    // Allocate and construct an operation to wrap the handler.
    typedef win_iocp_null_buffers_op<Handler> op;
    typename op::ptr p = { boost::addressof(handler),
      asio_handler_alloc_helpers::allocate(
        sizeof(op), handler), 0 };
    p.p = new (p.v) op(impl.cancel_token_, handler);

    start_reactor_op(impl, reactor::write_op, p.p);
    p.v = p.p = 0;
  }

  // Receive some data from the peer. Returns the number of bytes received.
  template <typename MutableBufferSequence>
  size_t receive(base_implementation_type& impl,
      const MutableBufferSequence& buffers,
      socket_base::message_flags flags, asio::error_code& ec)
  {
    buffer_sequence_adapter<asio::mutable_buffer,
        MutableBufferSequence> bufs(buffers);

    return socket_ops::sync_recv(impl.socket_, impl.state_,
        bufs.buffers(), bufs.count(), flags, bufs.all_empty(), ec);
  }

  // Wait until data can be received without blocking.
  size_t receive(base_implementation_type& impl, const null_buffers&,
      socket_base::message_flags, asio::error_code& ec)
  {
    // Wait for socket to become ready.
    socket_ops::poll_read(impl.socket_, ec);

    return 0;
  }

  // Start an asynchronous receive. The buffer for the data being received
  // must be valid for the lifetime of the asynchronous operation.
  template <typename MutableBufferSequence, typename Handler>
  void async_receive(base_implementation_type& impl,
      const MutableBufferSequence& buffers,
      socket_base::message_flags flags, Handler handler)
  {
    // Allocate and construct an operation to wrap the handler.
    typedef win_iocp_socket_recv_op<MutableBufferSequence, Handler> op;
    typename op::ptr p = { boost::addressof(handler),
      asio_handler_alloc_helpers::allocate(
        sizeof(op), handler), 0 };
    p.p = new (p.v) op(impl.state_, impl.cancel_token_, buffers, handler);

    buffer_sequence_adapter<asio::mutable_buffer,
        MutableBufferSequence> bufs(buffers);

    start_receive_op(impl, bufs.buffers(), bufs.count(), flags,
        (impl.state_ & socket_ops::stream_oriented) != 0 && bufs.all_empty(),
        p.p);
    p.v = p.p = 0;
  }

  // Wait until data can be received without blocking.
  template <typename Handler>
  void async_receive(base_implementation_type& impl, const null_buffers&,
      socket_base::message_flags flags, Handler handler)
  {
    // Allocate and construct an operation to wrap the handler.
    typedef win_iocp_null_buffers_op<Handler> op;
    typename op::ptr p = { boost::addressof(handler),
      asio_handler_alloc_helpers::allocate(
        sizeof(op), handler), 0 };
    p.p = new (p.v) op(impl.cancel_token_, handler);

    start_null_buffers_receive_op(impl, flags, p.p);
    p.v = p.p = 0;
  }

  // Helper function to restart an asynchronous accept operation.
  ASIO_DECL void restart_accept_op(socket_type s,
      socket_holder& new_socket, int family, int type, int protocol,
      void* output_buffer, DWORD address_length, operation* op);

protected:
  // Open a new socket implementation.
  ASIO_DECL asio::error_code do_open(
      base_implementation_type& impl, int family, int type,
      int protocol, asio::error_code& ec);

  // Assign a native socket to a socket implementation.
  ASIO_DECL asio::error_code do_assign(
      base_implementation_type& impl, int type,
      socket_type native_socket, asio::error_code& ec);

  // Helper function to start an asynchronous send operation.
  ASIO_DECL void start_send_op(base_implementation_type& impl,
      WSABUF* buffers, std::size_t buffer_count,
      socket_base::message_flags flags, bool noop, operation* op);

  // Helper function to start an asynchronous send_to operation.
  ASIO_DECL void start_send_to_op(base_implementation_type& impl,
      WSABUF* buffers, std::size_t buffer_count,
      const socket_addr_type* addr, int addrlen,
      socket_base::message_flags flags, operation* op);

  // Helper function to start an asynchronous receive operation.
  ASIO_DECL void start_receive_op(base_implementation_type& impl,
      WSABUF* buffers, std::size_t buffer_count,
      socket_base::message_flags flags, bool noop, operation* op);

  // Helper function to start an asynchronous null_buffers receive operation.
  ASIO_DECL void start_null_buffers_receive_op(
      base_implementation_type& impl,
      socket_base::message_flags flags, reactor_op* op);

  // Helper function to start an asynchronous receive_from operation.
  ASIO_DECL void start_receive_from_op(base_implementation_type& impl,
      WSABUF* buffers, std::size_t buffer_count, socket_addr_type* addr,
      socket_base::message_flags flags, int* addrlen, operation* op);

  // Helper function to start an asynchronous accept operation.
  ASIO_DECL void start_accept_op(base_implementation_type& impl,
      bool peer_is_open, socket_holder& new_socket, int family, int type,
      int protocol, void* output_buffer, DWORD address_length, operation* op);

  // Start an asynchronous read or write operation using the the reactor.
  ASIO_DECL void start_reactor_op(base_implementation_type& impl,
      int op_type, reactor_op* op);

  // Start the asynchronous connect operation using the reactor.
  ASIO_DECL void start_connect_op(base_implementation_type& impl,
      reactor_op* op, const socket_addr_type* addr, std::size_t addrlen);

  // Helper function to close a socket when the associated object is being
  // destroyed.
  ASIO_DECL void close_for_destruction(base_implementation_type& impl);

  // Update the ID of the thread from which cancellation is safe.
  ASIO_DECL void update_cancellation_thread_id(
      base_implementation_type& impl);

  // Helper function to get the reactor. If no reactor has been created yet, a
  // new one is obtained from the io_service and a pointer to it is cached in
  // this service.
  ASIO_DECL reactor& get_reactor();

  // Helper function to emulate InterlockedCompareExchangePointer functionality
  // for:
  // - very old Platform SDKs; and
  // - platform SDKs where MSVC's /Wp64 option causes spurious warnings.
  ASIO_DECL void* interlocked_compare_exchange_pointer(
      void** dest, void* exch, void* cmp);

  // Helper function to emulate InterlockedExchangePointer functionality for:
  // - very old Platform SDKs; and
  // - platform SDKs where MSVC's /Wp64 option causes spurious warnings.
  ASIO_DECL void* interlocked_exchange_pointer(void** dest, void* val);

  // The io_service used to obtain the reactor, if required.
  asio::io_service& io_service_;

  // The IOCP service used for running asynchronous operations and dispatching
  // handlers.
  win_iocp_io_service& iocp_service_;

  // The reactor used for performing connect operations. This object is created
  // only if needed.
  reactor* reactor_;

  // Mutex to protect access to the linked list of implementations. 
  asio::detail::mutex mutex_;

  // The head of a linked list of all implementations.
  base_implementation_type* impl_list_;
};

} // namespace detail
} // namespace asio

#include "asio/detail/pop_options.hpp"

#if defined(ASIO_HEADER_ONLY)
# include "asio/detail/impl/win_iocp_socket_service_base.ipp"
#endif // defined(ASIO_HEADER_ONLY)

#endif // defined(ASIO_HAS_IOCP)

#endif // ASIO_DETAIL_WIN_IOCP_SOCKET_SERVICE_BASE_HPP
