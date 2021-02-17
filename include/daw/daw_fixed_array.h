// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/header_libraries
//

#pragma once

#include <daw/daw_exchange.h>
#include <daw/daw_move.h>
#include <daw/daw_traits.h>

#include <algorithm>
#include <ciso646>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>

namespace daw {
	struct construct_for_overwrite_t {};
	inline constexpr construct_for_overwrite_t construct_for_overwrite{ };

	struct take_ownership_t {};
	inline constexpr take_ownership_t take_ownership{ };

	template<typename T>
	struct fixed_array {
		using value_type = T;
		using reference = T &;
		using const_reference = T const &;
		using pointer = T *;
		using const_pointer = T const *;
		using iterator = pointer;
		using const_iterator = const_pointer;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;

	private:
		size_type m_size = 0;
		value_type *m_data = nullptr;

	public:
		explicit constexpr fixed_array( ) noexcept = default;

		explicit fixed_array( size_type Size )
		  : m_size( Size )
		  , m_data( new T[Size]{ } ) {}

		explicit fixed_array( construct_for_overwrite_t, size_type Size )
		  : m_size( Size )
		  , m_data( new T[Size] ) {}

		explicit fixed_array( size_type Size, value_type const &def_value )
		  : fixed_array( construct_for_overwrite, Size ) {

			std::fill_n( m_data, m_size, def_value );
		}

		explicit fixed_array( pointer ptr, size_type sz )
		  : m_size( sz )
		  , m_data( ptr ) {}

		constexpr fixed_array( fixed_array &&other ) noexcept
		  : m_size( daw::exchange( other.m_size, 0ULL ) )
		  , m_data( daw::exchange( other.m_data, nullptr ) ) {}

		constexpr fixed_array &operator=( fixed_array &&rhs ) noexcept {
			if( this != &rhs ) {
				clear( );
				m_size = daw::exchange( rhs.m_size, 0ULL );
				m_data = daw::exchange( rhs.m_data, nullptr );
			}
			return *this;
		}

		fixed_array( fixed_array const &other )
		  : m_data( other.m_size == 0 ? nullptr : new value_type[other.m_size] )
		  , m_size( other.m_size ) {

			std::copy_n( other.m_data, m_size, m_data );
		}

		fixed_array &operator=( fixed_array const &rhs ) {
			if( this != &rhs ) {
				value_type *tmp = new value_type[m_size];
				std::copy_n( rhs.m_data, rhs.m_size, tmp );
				clear( );
				if( rhs.m_data == nullptr ) {
					return *this;
				}
				m_size = rhs.m_size;
				m_data = tmp;
			}
			return *this;
		}

		template<typename Iterator>
		fixed_array( Iterator first, size_type Size )
		  : m_size( Size )
		  , m_data( new value_type[m_size] ) {

			std::copy_n( first, m_size, m_data );
		}

		template<typename IteratorF, typename IteratorL,
		         std::enable_if_t<
		           daw::traits::has_subtraction_operator_v<IteratorL, IteratorF>,
		           std::nullptr_t> = nullptr>
		fixed_array( IteratorF first, IteratorL last )
		  : m_size( static_cast<size_type>( last - first ) )
		  , m_data( new value_type[m_size] ) {

			std::copy_n( first, m_size, m_data );
		}

		constexpr void clear( ) {
			delete[] m_data;
			m_size = 0;
		}

		~fixed_array( ) {
			clear( );
		}

		constexpr size_type size( ) const {
			return m_size;
		}

		constexpr pointer data( ) {
			return m_data;
		}

		constexpr const_pointer data( ) const {
			return m_data;
		}

		constexpr iterator begin( ) {
			return m_data;
		}

		constexpr const_iterator begin( ) const {
			return m_data;
		}

		constexpr const_iterator cbegin( ) const {
			return m_data;
		}

		constexpr iterator end( ) {
			return m_data + m_size;
		}

		constexpr const_iterator end( ) const {
			return m_data + m_size;
		}

		constexpr const_iterator cend( ) const {
			return m_data + m_size;
		}

		constexpr reference operator[]( size_type pos ) {
			return *( m_data + pos );
		}

		constexpr const_reference operator[]( size_type pos ) const {
			return *( m_data + pos );
		}

		constexpr reference front( ) {
			return *m_data;
		}

		constexpr const_reference front( ) const {
			return *m_data;
		}

		constexpr reference back( ) {
			return *( m_data + m_size - 1 );
		}

		constexpr const_reference back( ) const {
			return *( m_data + m_size - 1 );
		}

		std::unique_ptr<value_type[]> release( ) {
			m_size = 0;
			return std::unique_ptr<value_type[]>( std::exchange( m_data, nullptr ) );
		}
	}; // struct fixed_array
} // namespace daw
