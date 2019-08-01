// The MIT License (MIT)
//
// Copyright (c) 2016-2019 Darrell Wright
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

#include <memory>
#include <thread>

#include <daw/daw_scope_guard.h>
#include <daw/daw_utility.h>
#include <daw/parallel/daw_latch.h>

#include "impl/task_scheduler_impl.h"

namespace daw {
	struct unable_to_add_task_exception {};

	class task_scheduler {
		std::shared_ptr<impl::task_scheduler_impl> m_impl;

	public:
		explicit task_scheduler(
		  std::size_t num_threads = std::thread::hardware_concurrency( ),
		  bool block_on_destruction = true );

		template<typename Task, std::enable_if_t<std::is_invocable_v<Task>,
		                                         std::nullptr_t> = nullptr>
		[[nodiscard]] decltype( auto ) add_task( Task &&task ) noexcept {
			return m_impl->add_task( std::forward<Task>( task ) );
		}

		template<
		  typename Task, typename SharedLatch,
		  std::enable_if_t<daw::all_true_v<std::is_invocable_v<Task>,
		                                   daw::is_shared_latch_v<SharedLatch>>,
		                   std::nullptr_t> = nullptr>
		[[nodiscard]] decltype( auto ) add_task( Task &&task,
		                                         SharedLatch &&sem ) noexcept {
			return m_impl->add_task( std::forward<Task>( task ),
			                         std::forward<SharedLatch>( sem ) );
		}
		template<typename SharedLatch,
		         std::enable_if_t<daw::is_shared_latch_v<SharedLatch>,
		                          std::nullptr_t> = nullptr>
		[[nodiscard]] decltype( auto ) add_task( SharedLatch &&sem ) noexcept {
			return m_impl->add_task( [] {}, std::forward<SharedLatch>( sem ) );
		}

		void start( );
		void stop( bool block = true ) noexcept;
		bool started( ) const;
		size_t size( ) const;

		template<typename Function>
		[[nodiscard]] decltype( auto ) wait_for_scope( Function &&func ) {
			static_assert( std::is_invocable_v<Function>,
			               "Function passed to wait_for_scope must be callable "
			               "without an arugment. e.g. func( )" );

			auto const at_exit = daw::on_scope_exit(
			  [sem = ::daw::mutable_capture(
			     m_impl->start_temp_task_runners( ) )]( ) { sem->notify( ); } );
			return func( );
		}

		template<typename Waitable>
		void wait_for( Waitable &&waitable ) {
			static_assert(
			  impl::is_waitable_v<Waitable>,
			  "Waitable must have a wait( ) member. e.g. waitable.wait( )" );

			wait_for_scope( [&waitable]( ) { waitable.wait( ); } );
		}

		[[nodiscard]] explicit operator bool( ) const noexcept {
			return static_cast<bool>( m_impl ) && m_impl->started( );
		}
	}; // task_scheduler

	template<typename>
	struct is_task_scheduler : public std::false_type {};

	template<>
	struct is_task_scheduler<task_scheduler> : public std::true_type {};

	template<typename T>
	inline constexpr bool is_task_scheduler_v = is_task_scheduler<T>::value;

	task_scheduler get_task_scheduler( );

	/// Add a single task to the supplied task scheduler and notify supplied
	/// semaphore when complete
	///
	/// @param sem Semaphore to notify when task is completed
	/// @param task Task of form void( ) to run
	/// @param ts task_scheduler to add task to
	template<typename Task>
	[[nodiscard]] decltype( auto )
	schedule_task( daw::shared_latch sem, Task &&task,
	               task_scheduler ts = get_task_scheduler( ) ) {
		static_assert( std::is_invocable_v<Task>,
		               "Task task passed to schedule_task must be callable without "
		               "an arugment. e.g. task( )" );

		return ts.add_task( [task =
		                       daw::mutable_capture( std::forward<Task>( task ) ),
		                     sem = daw::mutable_capture( std::move( sem ) )]( ) {

			auto const at_exit = daw::on_scope_exit( [&sem]( ) { sem->notify( ); } );

			daw::invoke( daw::move( *task ) );
		} );
	}

	template<typename Task>
	[[nodiscard]] daw::shared_latch
	create_waitable_task( Task &&task,
	                      task_scheduler ts = get_task_scheduler( ) ) {
		static_assert( std::is_invocable_v<Task>,
		               "Task task passed to create_waitable_task must be callable "
		               "without an arugment. "
		               "e.g. task( )" );
		auto sem = daw::shared_latch( );
		if( !schedule_task( sem, std::forward<Task>( task ), daw::move( ts ) ) ) {
			// TODO, I don't like this but I don't want to change the return value to
			// express that we failed to add the task... yet
			sem.notify( );
		}
		return sem;
	}

	/// Run concurrent tasks and return when completed
	///
	/// @param tasks callable items of the form void( )
	/// @returns a semaphore that will stop waiting when all tasks complete
	template<typename... Tasks>
	[[nodiscard]] daw::shared_latch create_task_group( Tasks &&... tasks ) {
		static_assert( impl::are_tasks_v<Tasks...>,
		               "Tasks passed to create_task_group must be callable without "
		               "an arugment. e.g. task( )" );
		auto ts = get_task_scheduler( );
		auto sem = daw::shared_latch( sizeof...( tasks ) );

		auto const st = [&]( auto &&task ) {
			if( !schedule_task( sem, std::forward<decltype( task )>( task ), ts ) ) {
				// TODO, I don't like this but I don't want to change the return value
				// to express that we failed to add the task... yet
				sem.notify( );
			}
			return 0;
		};

		Unused( ( st( std::forward<Tasks>( tasks ) ) + ... ) );

		return sem;
	}

	/// Run concurrent tasks and return when completed
	///
	/// @param tasks callable items of the form void( )
	template<typename... Tasks>
	void invoke_tasks( task_scheduler ts, Tasks &&... tasks ) {
		ts.wait_for( create_task_group( std::forward<Tasks>( tasks )... ) );
	}

	template<typename... Tasks>
	void invoke_tasks( Tasks &&... tasks ) {
		static_assert( impl::are_tasks_v<Tasks...>,
		               "Tasks passed to invoke_tasks must be callable without an "
		               "arugment. e.g. task( )" );
		invoke_tasks( get_task_scheduler( ), std::forward<Tasks>( tasks )... );
	}

	template<typename Function>
	[[nodiscard]] decltype( auto )
	wait_for_scope( Function &&func, task_scheduler ts = get_task_scheduler( ) ) {
		static_assert( std::is_invocable_v<Function>,
		               "Function passed to wait_for_scope must be callable without "
		               "an arugment. e.g. func( )" );
		return ts.wait_for_scope( std::forward<Function>( func ) );
	}
} // namespace daw