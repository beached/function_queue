// Copyright (c) Darrell Wright
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/beached/header_libraries
//

#pragma once

#include <daw/cpp_17.h>
#include <daw/daw_do_not_optimize.h>
#include <daw/daw_expected.h>
#include <daw/daw_move.h>
#include <daw/daw_string_view.h>
#include <daw/daw_traits.h>

#include <algorithm>
#include <chrono>
#include <ciso646>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if not( defined( __cpp_exceptions ) or defined( __EXCEPTIONS ) or             \
         defined( _CPPUNWIND ) ) or                                            \
  defined( DAW_NO_EXCEPTIONS )
#ifdef DAW_USE_EXCEPTIONS
#undef DAW_USE_EXCEPTIONS
#endif
#else
#ifndef DAW_USE_EXCEPTIONS
#define DAW_USE_EXCEPTIONS
#endif
#endif

namespace daw {
	namespace benchmark_impl {
		using second_duration = std::chrono::duration<double>;
	} // namespace benchmark_impl

	template<typename F>
	[[maybe_unused]] static inline double benchmark( F &&func ) {
		static_assert( std::is_invocable_v<F>, "func must accept no arguments" );
		auto start = std::chrono::steady_clock::now( );
		(void)std::forward<F>( func )( );
		auto finish = std::chrono::steady_clock::now( );
		benchmark_impl::second_duration duration = finish - start;
		return duration.count( );
	}

	namespace utility {
		template<typename T>
		[[maybe_unused, nodiscard]] inline std::string
		format_seconds( T t, size_t prec = 0 ) {
			std::stringstream ss;
			ss << std::setprecision( static_cast<int>( prec ) ) << std::fixed;
			auto val = static_cast<double>( t ) * 1'000'000'000'000'000.0;
			if( val < 1000 ) {
				ss << val << "fs";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000 ) {
				ss << val << "ps";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000 ) {
				ss << val << "ns";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000 ) {
				ss << val << "us";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000 ) {
				ss << val << "ms";
				return ss.str( );
			}
			val /= 1000.0;
			ss << val << "s";
			return ss.str( );
		}

		template<typename Bytes, typename Time = double>
		[[maybe_unused]] inline std::string
		to_bytes_per_second( Bytes bytes, Time t = 1.0, size_t prec = 1 ) {
			std::stringstream ss;
			ss << std::setprecision( static_cast<int>( prec ) ) << std::fixed;
			auto val = static_cast<double>( bytes ) / static_cast<double>( t );
			if( val < 1000.0 ) {
				ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "bytes";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000.0 ) {
				ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "KB";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000.0 ) {
				ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "MB";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000.0 ) {
				ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "GB";
				return ss.str( );
			}
			val /= 1000.0;
			if( val < 1000.0 ) {
				ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "TB";
				return ss.str( );
			}
			val /= 1000.0;
			ss << ( static_cast<double>( val * 100.0 ) / 100 ) << "PB";
			return ss.str( );
		}
	} // namespace utility

	template<typename Func>
	[[maybe_unused]] static void
	show_benchmark( size_t data_size_bytes, std::string const &title, Func &&func,
	                size_t data_prec = 1, size_t time_prec = 0,
	                size_t item_count = 1 ) {
		double const t = benchmark( std::forward<Func>( func ) );
		double const t_per_item = t / static_cast<double>( item_count );
		std::cout << title << ": took " << utility::format_seconds( t, time_prec )
		          << ' ';
		if( item_count > 1 ) {
			std::cout << "or " << utility::format_seconds( t_per_item, time_prec )
			          << " per item to process ";
		}
		std::cout << utility::to_bytes_per_second( data_size_bytes, 1.0, data_prec )
		          << " at "
		          << utility::to_bytes_per_second( data_size_bytes, t, data_prec )
		          << "/s\n";
	}

	template<typename Test, typename... Args>
	[[maybe_unused]] static auto
	bench_test( std::string const &title, Test &&test_callable, Args &&...args ) {
		auto const start = std::chrono::steady_clock::now( );
		auto result = daw::expected_from_code( std::forward<Test>( test_callable ),
		                                       std::forward<Args>( args )... );
		auto const finish = std::chrono::steady_clock::now( );
		benchmark_impl::second_duration const duration = finish - start;
		std::cout << title << " took "
		          << utility::format_seconds( duration.count( ), 2 ) << '\n';
		return result;
	}

	template<typename Test, typename... Args>
	[[maybe_unused]] static auto
	bench_test2( std::string const &title, Test &&test_callable,
	             size_t item_count, Args &&...args ) {
		auto const start = std::chrono::steady_clock::now( );
		auto result = daw::expected_from_code( std::forward<Test>( test_callable ),
		                                       std::forward<Args>( args )... );
		auto const finish = std::chrono::steady_clock::now( );
		benchmark_impl::second_duration const duration = finish - start;
		std::cout << title << " took "
		          << utility::format_seconds( duration.count( ), 2 );
		if( item_count > 1 ) {
			std::cout << " to process " << item_count << " items at "
			          << utility::format_seconds( ( duration / item_count ).count( ),
			                                      2 )
			          << " per item\n";
		} else {
			std::cout << '\n';
		}
		return result;
	}

	/***
	 *
	 * @tparam Runs Number of runs to do
	 * @tparam delem delemiter in output
	 * @tparam Test Callable type to benchmark
	 * @tparam Args types passed to callable
	 * @param title Title of benchmark
	 * @param test_callable callable to benchmark
	 * @param args arg values to pass to callable
	 * @return last value from callable
	 */
	template<size_t Runs, char delem = '\n', typename Test, typename... Args>
	[[maybe_unused]] static auto bench_n_test( std::string const &title,
	                                           Test &&test_callable,
	                                           Args &&...args ) noexcept {
		static_assert( Runs > 0 );
		using result_t = daw::remove_cvref_t<decltype( daw::expected_from_code(
		  test_callable, std::forward<Args>( args )... ) )>;

		result_t result{ };

		double base_time = std::numeric_limits<double>::max( );
		{
			for( size_t n = 0; n < 1000; ++n ) {
				daw::do_not_optimize( args... );

				int a = 0;
				daw::do_not_optimize( a );
				auto const start = std::chrono::steady_clock::now( );
				auto r = daw::expected_from_code( [a]( ) mutable {
					daw::do_not_optimize( a );
					return a * a;
				} );
				auto const finish = std::chrono::steady_clock::now( );
				daw::do_not_optimize( r );
				auto const duration =
				  benchmark_impl::second_duration( finish - start ).count( );
				if( duration < base_time ) {
					base_time = duration;
				}
			}
		}
		double min_time = std::numeric_limits<double>::max( );
		double max_time = 0.0;

		auto const total_start = std::chrono::steady_clock::now( );
		for( size_t n = 0; n < Runs; ++n ) {
			daw::do_not_optimize( args... );
			auto const start = std::chrono::steady_clock::now( );

			result =
			  daw::expected_from_code( std::forward<Test>( test_callable ), args... );

			auto const finish = std::chrono::steady_clock::now( );
			daw::do_not_optimize( result );
			auto const duration =
			  benchmark_impl::second_duration( finish - start ).count( );
			if( duration < min_time ) {
				min_time = duration;
			}
			if( duration > max_time ) {
				max_time = duration;
			}
		}
		auto const total_finish = std::chrono::steady_clock::now( );
		min_time -= base_time;
		max_time -= base_time;
		auto total_time =
		  benchmark_impl::second_duration( total_finish - total_start ).count( ) -
		  static_cast<double>( Runs ) * base_time;

		auto avg_time = [&] {
			if constexpr( Runs >= 10 ) {
				return ( total_time - max_time ) / static_cast<double>( Runs - 1 );
			} else if constexpr( Runs > 1 ) {
				return total_time / static_cast<double>( Runs );
			} else /* Runs == 1 */ {
				return ( total_time - max_time ) / 1.0;
			}
		}( );

		avg_time -= base_time;
		std::cout << title << delem << "	runs: " << Runs << delem
		          << "	total: " << utility::format_seconds( total_time, 2 )
		          << delem << "	avg:   " << utility::format_seconds( avg_time, 2 )
		          << delem << "	min:   " << utility::format_seconds( min_time, 2 )
		          << delem << "	max:   " << utility::format_seconds( max_time, 2 )
		          << '\n';
		return result;
	}

	/***
	 *
	 * @tparam Runs Number of runs
	 * @tparam delem Delemiter in output
	 * @tparam Validator Callable to validate results
	 * @tparam Function Callable type to be timed
	 * @tparam Args types to pass to callable
	 * @param title Title for output
	 * @param bytes Size of data in bytes
	 * @param validator validatio object that takes func's result as arg
	 * @param func Callable value to bench
	 * @param args args values to pass to func
	 * @return last result timing counts of runs
	 */
	template<size_t Runs, char delem = '\n', typename Validator,
	         typename Function, typename... Args>
	[[maybe_unused]] static std::array<double, Runs>
	bench_n_test_mbs2( std::string const &title, size_t bytes,
	                   Validator &&validator, Function &&func,
	                   Args &&...args ) noexcept {
		static_assert( Runs > 0 );
		std::cout << "test: " << title << '\n';
		auto results = std::array<double, Runs>{ };

		double base_time = std::numeric_limits<double>::max( );
		{
			for( size_t n = 0; n < 1000; ++n ) {
				daw::do_not_optimize( args... );

				int a = 0;
				daw::do_not_optimize( a );
				auto const start = std::chrono::steady_clock::now( );
				auto r = daw::expected_from_code( [a]( ) mutable {
					daw::do_not_optimize( a );
					return a * a;
				} );
				auto const finish = std::chrono::steady_clock::now( );
				daw::do_not_optimize( r );
				auto const duration =
				  benchmark_impl::second_duration( finish - start ).count( );
				if( duration < base_time ) {
					base_time = duration;
				}
			}
		}
		// auto const total_start = std::chrono::steady_clock::now( );
		benchmark_impl::second_duration valid_time = std::chrono::seconds( 0 );
		auto tp_args = std::tuple{ DAW_FWD( args )... };
		for( size_t n = 0; n < Runs; ++n ) {
			daw::do_not_optimize( tp_args );
			auto cur_args = tp_args;
			auto const start = std::chrono::steady_clock::now( );
			auto const result = std::apply( func, cur_args );
			auto const finish = std::chrono::steady_clock::now( );
			daw::do_not_optimize( result );
			auto const valid_start = std::chrono::steady_clock::now( );
			if( not validator( result ) ) {
				std::cerr << "Error validating result\n";
				std::abort( );
			}
			valid_time += benchmark_impl::second_duration(
			  std::chrono::steady_clock::now( ) - valid_start );

			auto const duration =
			  benchmark_impl::second_duration( finish - start ).count( );
			results[n] = duration;
		}
		return results;
	}

	template<size_t Runs, char delem = '\n', typename Test, typename... Args>
	[[maybe_unused]] static auto
	bench_n_test_mbs( std::string const &title, size_t bytes,
	                  Test &&test_callable, Args const &...args ) noexcept {
		static_assert( Runs > 0 );
		using result_t = daw::remove_cvref_t<decltype(
		  daw::expected_from_code( test_callable, args... ) )>;

		result_t result{ };

		double base_time = std::numeric_limits<double>::max( );
		{
			for( size_t n = 0; n < 1000; ++n ) {
				daw::do_not_optimize( args... );

				intmax_t a = 0;
				daw::do_not_optimize( a );
				auto const start = std::chrono::steady_clock::now( );
				auto r = daw::expected_from_code( [a]( ) mutable {
					daw::do_not_optimize( a );
					return a * a;
				} );
				auto const finish = std::chrono::steady_clock::now( );
				daw::do_not_optimize( r );
				auto const duration =
				  benchmark_impl::second_duration( finish - start ).count( );
				if( duration < base_time ) {
					base_time = duration;
				}
			}
		}
		double min_time = std::numeric_limits<double>::max( );
		double max_time = 0.0;

		auto const total_start = std::chrono::steady_clock::now( );
		for( size_t n = 0; n < Runs; ++n ) {
			std::chrono::time_point<std::chrono::steady_clock,
			                        std::chrono::nanoseconds>
			  start = std::chrono::steady_clock::now( );
			result = daw::expected_from_code( test_callable, args... );
			auto const finish = std::chrono::steady_clock::now( );
			daw::do_not_optimize( result );
			auto const duration =
			  benchmark_impl::second_duration( finish - start ).count( );
			if( duration < min_time ) {
				min_time = duration;
			}
			if( duration > max_time ) {
				max_time = duration;
			}
			if( not result.has_value( ) ) {
				break;
			}
		}
		auto const total_finish = std::chrono::steady_clock::now( );
		min_time -= base_time;
		max_time -= base_time;
		auto total_time =
		  benchmark_impl::second_duration( total_finish - total_start ).count( ) -
		  static_cast<double>( Runs ) * base_time;

		auto avg_time = [&] {
			if constexpr( Runs >= 10 ) {
				return ( total_time - max_time ) / static_cast<double>( Runs - 1 );
			} else if constexpr( Runs > 1 ) {
				return total_time / static_cast<double>( Runs );
			} else /* Runs == 1 */ {
				return ( total_time - max_time ) / 1.0;
			}
		}( );
		avg_time -= base_time;
		std::cout << title << delem;
		if( not result.has_value( ) ) {
			return result;
		}
		std::cout << "	runs:        " << Runs << delem
		          << "	total:       " << utility::format_seconds( total_time, 2 )
		          << delem
		          << "	min:         " << utility::format_seconds( min_time, 2 )
		          << " -> " << utility::to_bytes_per_second( bytes, min_time, 2 )
		          << "/s" << delem
		          << "	avg:         " << utility::format_seconds( avg_time, 2 )
		          << " -> " << utility::to_bytes_per_second( bytes, avg_time, 2 )
		          << "/s" << delem
		          << "	max:         " << utility::format_seconds( max_time, 2 )
		          << " -> " << utility::to_bytes_per_second( bytes, max_time, 2 )
		          << "/s" << delem << "	runs/second: " << ( 1.0 / min_time )
		          << '\n';
		return result;
	} // namespace daw

	/***
	 *
	 * @tparam Runs Number of runs
	 * @tparam Function Callable type to be timed
	 * @tparam Args types to pass to callable
	 * @param func Callable value to bench
	 * @param args args values to pass to func
	 * @return last result timing counts of runs
	 */
	template<size_t Runs, typename Function, typename... Args>
	[[maybe_unused]] static std::vector<std::chrono::nanoseconds>
	bench_n_test_json( Function &&func, Args &&...args ) noexcept {
		static_assert( Runs > 0 );
		std::vector<std::chrono::nanoseconds> results( Runs );

		auto base_time =
		  std::chrono::nanoseconds( std::numeric_limits<long long>::max( ) );
		{
			for( size_t n = 0; n < 1000; ++n ) {
				daw::do_not_optimize( args... );

				int a = 0;
				daw::do_not_optimize( a );
				auto const start = std::chrono::steady_clock::now( );
				auto r = daw::expected_from_code( [a]( ) mutable {
					auto const b = a;
					daw::do_not_optimize( a );
					return a + b;
				} );
				auto const finish = std::chrono::steady_clock::now( );
				daw::do_not_optimize( r );
				auto const duration = std::chrono::nanoseconds( finish - start );
				if( duration < base_time ) {
					base_time = duration;
				}
			}
		}
		// using result_t = daw::remove_cvref_t<decltype( func( args... ) )>;

		for( size_t n = 0; n < Runs; ++n ) {
			auto const start = std::chrono::steady_clock::now( );
			func( args... );
			auto const finish = std::chrono::steady_clock::now( );

			auto const duration =
			  std::chrono::nanoseconds( finish - start ) - base_time;
			results[n] = duration;
		}
		return results;
	}

	/***
	 *
	 * @tparam Runs Number of runs
	 * @tparam Validator Callable to validate results
	 * @tparam Function Callable type to be timed
	 * @tparam Args types to pass to callable
	 * @param validator validatio object that takes func's result as arg
	 * @param func Callable value to bench
	 * @param args args values to pass to func
	 * @return last result timing counts of runs
	 */
	template<size_t Runs, typename Validator, typename Function, typename... Args>
	[[maybe_unused]] static std::vector<std::chrono::nanoseconds>
	bench_n_test_json_val( Validator &&validator, Function &&func,
	                       Args &&...args ) noexcept {
		static_assert( Runs > 0 );
		std::vector<std::chrono::nanoseconds> results( Runs );

		auto base_time =
		  std::chrono::nanoseconds( std::numeric_limits<long long>::max( ) );
		{
			for( size_t n = 0; n < 1000; ++n ) {
				daw::do_not_optimize( args... );

				int a = 0;
				daw::do_not_optimize( a );
				auto const start = std::chrono::steady_clock::now( );
				auto r = daw::expected_from_code( [a]( ) mutable {
					auto const b = a;
					daw::do_not_optimize( a );
					return a + b;
				} );
				auto const finish = std::chrono::steady_clock::now( );
				daw::do_not_optimize( r );
				auto const duration = std::chrono::nanoseconds( finish - start );
				if( duration < base_time ) {
					base_time = duration;
				}
			}
		}
		using result_t = daw::remove_cvref_t<decltype( func( args... ) )>;

		for( size_t n = 0; n < Runs; ++n ) {
			auto const start = std::chrono::steady_clock::now( );
			result_t result = *daw::expected_from_code( func, args... );
			auto const finish = std::chrono::steady_clock::now( );
			daw::do_not_optimize( result );
			if( not validator( result ) ) {
				std::cerr << "Error validating result\n";
				std::abort( );
			}

			auto const duration =
			  std::chrono::nanoseconds( finish - start ) - base_time;
			results[n] = duration;
		}
		return results;
	}

	namespace benchmark_impl {
		template<typename T>
		using detect_streamable =
		  decltype( std::declval<std::ios &>( ) << std::declval<T const &>( ) );

		template<typename T>
		using is_streamable = daw::is_detected<detect_streamable, T>;

		template<
		  typename T, typename U,
		  std::enable_if_t<std::conjunction_v<is_streamable<T>, is_streamable<U>>,
		                   std::nullptr_t> = nullptr>
		[[maybe_unused]] void output_expected_error( T &&expected_result,
		                                             U &&result ) {
			std::cerr << "Invalid result. Expecting '" << expected_result
			          << "' but got '" << result << "'\n";
		}

		template<typename T, typename U,
		         std::enable_if_t<std::conjunction_v<not_trait<is_streamable<T>>,
		                                             not_trait<is_streamable<U>>>,
		                          std::nullptr_t> = nullptr>
		[[maybe_unused]] constexpr void output_expected_error( T &&, U && ) {
			std::cerr << "Invalid or unexpected result\n";
		}
	} // namespace benchmark_impl

	template<typename T, typename U>
	[[maybe_unused]] static constexpr void expecting( T &&expected_result,
	                                                  U &&result ) {
		if( not( expected_result == result ) ) {
#ifdef __VERSION__
			if constexpr( not daw::string_view( __VERSION__ )
			                    .starts_with( daw::string_view(
			                      "4.2.1 Compatible Apple LLVM" ) ) ) {
				benchmark_impl::output_expected_error( expected_result, result );
			}
#endif
			std::abort( );
		}
	}

	template<typename Bool>
	[[maybe_unused]] static constexpr void
	expecting( Bool const &expected_result ) {
		if( not static_cast<bool>( expected_result ) ) {
			std::cerr << "Invalid result. Expecting true\n";
			std::abort( );
		}
	}

	template<typename Bool, typename String>
	[[maybe_unused]] static constexpr void
	expecting_message( Bool const &expected_result, String &&message ) {
		if( not static_cast<bool>( expected_result ) ) {
			std::cerr << message << '\n';
			std::abort( );
		}
	}

	namespace benchmark_impl {
		struct always_true {
			template<typename... Args>
			[[nodiscard, maybe_unused]] constexpr bool
			operator( )( Args &&... ) noexcept {
				return true;
			}
		};
	} // namespace benchmark_impl

	template<typename Exception = std::exception, typename Expression,
	         typename Predicate = benchmark_impl::always_true,
	         std::enable_if_t<std::is_invocable_v<Predicate, Exception>,
	                          std::nullptr_t> = nullptr>
	[[maybe_unused]] static void
	expecting_exception( Expression &&expression,
	                     Predicate &&pred = Predicate{ } ) {
#ifdef DAW_USE_EXCEPTIONS
		try {
#endif
			(void)std::forward<Expression>( expression )( );
#ifdef DAW_USE_EXCEPTIONS
		} catch( Exception const &ex ) {
			if( std::forward<Predicate>( pred )( ex ) ) {
				return;
			}
			std::cerr << "Failed predicate\n";
		} catch( ... ) {
			std::cerr << "Unexpected exception\n";
			throw;
		}
		std::abort( );
#endif
	}
} // namespace daw

#ifdef DAW_USE_EXCEPTIONS
#undef DAW_USE_EXCEPTIONS
#endif
