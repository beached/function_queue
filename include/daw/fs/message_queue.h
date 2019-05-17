// The MIT License (MIT)
//
// Copyright (c) 2017-2019 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include <boost/lockfree/queue.hpp>

namespace daw::parallel {
	enum class push_back_result : bool { failed, success };

	template<typename T>
	class locking_circular_buffer {
		using value_type = std::optional<T>;
		struct members_t {
			value_type *m_values;
			std::mutex m_mutex{};
			std::condition_variable m_not_empty{};
			std::condition_variable m_not_full{};
			size_t m_front = 0;
			size_t m_back = 0;
			size_t m_size;
			bool m_is_full = false;

			members_t( size_t queue_size ) noexcept
			  : m_values( new value_type[queue_size] )
			  , m_size( queue_size ) {}

			~members_t( ) noexcept {
				delete[] m_values;
			}

			members_t( members_t const & ) = delete;
			members_t( members_t && ) = delete;
			members_t &operator=( members_t const & ) = delete;
			members_t &operator=( members_t && ) = delete;
		};
		std::unique_ptr<members_t> m_data = std::make_unique<members_t>( );

		bool empty( ) const {
			return !m_data->m_is_full and m_data->m_front == m_data->m_back;
		}

		bool full( ) const {
			return m_data->m_is_full;
		}

	public:
		locking_circular_buffer( size_t queue_size )
		  : m_data( std::make_unique<members_t>( queue_size ) ) {}

		std::optional<T> try_pop_front( ) {
			auto lck = std::unique_lock( m_data->m_mutex, std::try_to_lock );
			if( !lck.owns_lock( ) or empty( ) ) {
				return {};
			}
			m_data->m_is_full = false;
			assert( m_data->m_values[m_data->m_front] );
			auto result =
			  std::exchange( m_data->m_values[m_data->m_front], std::optional<T>{} );
			m_data->m_front = ( m_data->m_front + 1 ) % m_data->m_size;
			return result;
		}

		template<typename Bool>
		std::optional<T> pop_front( Bool &&can_continue ) {
			auto lck = std::unique_lock( m_data->m_mutex );
			if( empty( ) ) {
				m_data->m_not_empty.wait(
				  lck, [&]( ) { return can_continue and !empty( ); } );
			}
			if( !can_continue ) {
				return {};
			}
			auto const oe = daw::on_scope_exit( [&]( ) {
				m_data->m_is_full = false;
				m_data->m_not_full.notify_one( );
			} );

			auto result =
			  std::exchange( m_data->m_values[m_data->m_front], std::optional<T>{} );
			m_data->m_front = ( m_data->m_front + 1 ) % m_data->m_size;
			return result;
		}

		template<typename U, typename Bool>
		void push_back( U &&value, Bool &&can_continue ) {
			static_assert( std::is_convertible_v<U, T> );
			auto lck = std::unique_lock( m_data->m_mutex, std::try_to_lock );
			if( !lck.owns_lock( ) or full( ) ) {
				m_data->m_not_full.wait( lck,
				                         [&]( ) { return can_continue and !full( ); } );
			}
			if( !can_continue ) {
				return;
			}
			auto const oe = daw::on_scope_exit( [&]( ) {
				m_data->m_is_full = m_data->m_front == m_data->m_back;
				m_data->m_not_empty.notify_one( );
			} );

			m_data->m_values[m_data->m_back] = std::forward<U>( value );
			m_data->m_back = ( m_data->m_back + 1 ) % m_data->m_size;
		}

		template<typename U>
		push_back_result try_push_back( U &&value ) {
			static_assert( std::is_convertible_v<U, T> );
			auto lck = std::unique_lock( m_data->m_mutex, std::try_to_lock );
			if( !lck.owns_lock( ) or full( ) ) {
				m_data->m_not_full.wait( lck, [&]( ) { return !full( ); } );
			}
			assert( !m_data->m_values[m_data->m_back] );
			auto const oe =
			  daw::on_scope_exit( [&]( ) { m_data->m_not_empty.notify_one( ); } );

			m_data->m_values[m_data->m_back] = std::forward<U>( value );
			m_data->m_back = ( m_data->m_back + 1 ) % m_data->m_size;
			m_data->m_is_full = m_data->m_front == m_data->m_back;
			return push_back_result::success;
		}
	};
} // namespace daw::parallel
