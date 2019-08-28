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

#include <exception>
#include <memory>
#include <thread>

#include <daw/daw_scope_guard.h>
#include <daw/daw_utility.h>
#include <daw/parallel/daw_latch.h>
#include <daw/parallel/daw_locked_value.h>

#include "impl/task.h"
#include "message_queue.h"

namespace daw {
	template<typename... Tasks>
	constexpr bool are_tasks_v = daw::all_true_v<std::is_invocable_v<Tasks>...>;

	template<typename Waitable>
	using is_waitable_detector = decltype( std::declval<Waitable &>( ).wait( ) );

	template<typename Waitable>
	constexpr bool is_waitable_v =
	  daw::is_detected_v<is_waitable_detector, std::remove_reference_t<Waitable>>;

	template<typename... Args>
	inline std::optional<task_t> make_task_ptr( Args &&... args ) {
		return std::optional<task_t>( std::in_place,
		                              std::forward<Args>( args )... );
	}
	struct unable_to_add_task_exception : std::exception {
		unable_to_add_task_exception( ) = default;

		char const *what( ) const noexcept override;
	};

	class task_scheduler {
		using task_queue_t =
		  daw::parallel::locking_circular_buffer<daw::task_t, 1024>;

		class member_data_t {
			daw::lockable_value_t<std::vector<std::thread>> m_threads{};
			daw::lockable_value_t<std::unordered_map<std::thread::id, size_t>>
			  m_thread_map{};
			size_t const m_num_threads;        // from ctor
			std::vector<task_queue_t> m_tasks; // from ctor
			std::atomic<size_t> m_task_count{0};
			daw::lockable_value_t<std::list<std::optional<std::thread>>>
			  m_other_threads{};
			std::atomic<size_t> m_current_id{0};
			std::atomic_bool m_continue = false;
			bool m_block_on_destruction; // from ctor

			friend task_scheduler;

			member_data_t( std::size_t num_threads, bool block_on_destruction );
		};
		std::shared_ptr<member_data_t> m_data = std::shared_ptr<member_data_t>(
		  new member_data_t( std::thread::hardware_concurrency( ), true ) );

		task_scheduler( std::shared_ptr<member_data_t> &&sptr )
		  : m_data( daw::move( sptr ) ) {}

		[[nodiscard]] inline auto get_handle( ) {
			class handle_t {
				std::weak_ptr<member_data_t> m_handle;

				explicit handle_t( std::weak_ptr<member_data_t> wptr )
				  : m_handle( wptr ) {}

				friend task_scheduler;

			public:
				bool expired( ) const {
					return m_handle.expired( );
				}

				std::optional<task_scheduler> lock( ) const {
					if( auto lck = m_handle.lock( ); lck ) {
						return task_scheduler( daw::move( lck ) );
					}
					return {};
				}
			};

			return handle_t( static_cast<std::weak_ptr<member_data_t>>( m_data ) );
		}

		[[nodiscard]] std::optional<daw::task_t>
		wait_for_task_from_pool( size_t id );

		[[nodiscard]] static std::vector<task_queue_t>
		make_task_queues( size_t count ) {
			std::vector<task_queue_t> result{};
			result.reserve( count );
			for( size_t n = 0; n < count; ++n ) {
				result.emplace_back( );
			}
			return result;
		}
		[[nodiscard]] bool send_task( std::optional<task_t> &&tsk, size_t id );

		template<typename Task, std::enable_if_t<std::is_invocable_v<Task>,
		                                         std::nullptr_t> = nullptr>
		[[nodiscard]] bool add_task( Task &&task, size_t id ) {
			return send_task(
			  make_task_ptr(
			    [wself = get_handle( ),
			     task = daw::mutable_capture( std::forward<Task>( task ) ), id]( ) {
				    if( auto self = wself.lock( ); self ) {
					    ::daw::move ( *task )( );
					    while( self->m_data->m_continue and self->run_next_task( id ) ) {}
				    }
			    } ),
			  id );
		}

		template<typename Task>
		[[nodiscard]] decltype( auto ) add_task( Task &&task, daw::shared_latch sem,
		                                         size_t id ) {
			auto tsk = [wself = get_handle( ), id,
			            task =
			              daw::mutable_capture( std::forward<Task>( task ) )]( ) {
				if( auto self = wself.lock( ); self ) {
					::daw::move ( *task )( );
					while( self->m_data->m_continue and self->run_next_task( id ) ) {}
				}
			};
			return send_task( make_task_ptr( daw::move( tsk ), std::move( sem ) ),
			                  id );
		}

		template<typename Handle>
		void task_runner( size_t id, Handle hnd ) {

			auto self = hnd.lock( );
			if( not self ) {
				return;
			}
			auto self2 = *get_handle( ).lock( );
			while( self2.m_data->m_continue ) {
				run_task( self2.wait_for_task_from_pool( id ) );
			}
		}

		template<typename Handle>
		void task_runner( size_t id, Handle hnd,
		                  std::optional<daw::shared_latch> sem ) {

			// The self.lock( ) determines where or not the
			// task_scheduler_impl has destructed yet and keeps it alive while
			// we use members
			if( hnd.expired( ) ) {
				return;
			}
			while( not sem->try_wait( ) ) {
				if( auto self = hnd.lock( ); not( self or self->m_data->m_continue ) ) {
					return;
				} else {
					run_task( self->wait_for_task_from_pool( id ) );
				}
			}
		}

		void run_task( std::optional<task_t> &&tsk ) noexcept;

		[[nodiscard]] size_t get_task_id( );

	public:
		task_scheduler( );
		task_scheduler( std::size_t num_threads );
		task_scheduler( std::size_t num_threads, bool block_on_destruction );
		task_scheduler( std::size_t num_threads, bool block_on_destruction,
		                bool auto_start );

		task_scheduler( task_scheduler && ) = default;
		task_scheduler &operator=( task_scheduler && ) = default;
		task_scheduler( task_scheduler const & ) = default;
		task_scheduler &operator=( task_scheduler const & ) = default;
		~task_scheduler( );

		template<typename Task, std::enable_if_t<std::is_invocable_v<Task>,
		                                         std::nullptr_t> = nullptr>
		[[nodiscard]] decltype( auto ) add_task( Task &&task ) {
			static_assert(
			  std::is_invocable_v<Task>,
			  "Task must be callable without arguments (e.g. task( );)" );

			return add_task( std::forward<Task>( task ), get_task_id( ) );
		}

		template<typename Task>
		[[nodiscard]] decltype( auto ) add_task( Task &&task,
		                                         daw::shared_latch sem ) {
			static_assert(
			  std::is_invocable_v<Task>,
			  "Task must be callable without arguments (e.g. task( );)" );

			return add_task( std::forward<Task>( task ), std::move( sem ),
			                 get_task_id( ) );
		}

		[[nodiscard]] bool run_next_task( size_t id );

		void start( );
		void stop( bool block = true ) noexcept;

		[[nodiscard]] bool started( ) const;

		[[nodiscard]] size_t size( ) const {
			return m_data->m_tasks.size( );
		}

		[[nodiscard]] daw::shared_latch
		start_temp_task_runners( size_t task_count = 1U );

		[[nodiscard]] inline decltype( auto )
		add_task( daw::shared_latch &&sem ) noexcept {
			return add_task( [] {}, daw::move( sem ) );
		}

		[[nodiscard]] inline decltype( auto )
		add_task( daw::shared_latch const &sem ) noexcept {
			return add_task( [] {}, sem );
		}

		template<typename Function>
		[[nodiscard]] decltype( auto ) wait_for_scope( Function &&func ) {
			static_assert( std::is_invocable_v<Function>,
			               "Function passed to wait_for_scope must be callable "
			               "without an arugment. e.g. func( )" );

			auto const at_exit = daw::on_scope_exit(
			  [sem = ::daw::mutable_capture( start_temp_task_runners( ) )]( ) {
				  sem->notify( );
			  } );
			return std::forward<Function>( func )( );
		}

		template<typename Waitable>
		void wait_for( Waitable &&waitable ) {
			static_assert(
			  is_waitable_v<Waitable>,
			  "Waitable must have a wait( ) member. e.g. waitable.wait( )" );

			wait_for_scope( [&waitable]( ) { waitable.wait( ); } );
		}

		[[nodiscard]] explicit operator bool( ) const noexcept {
			return started( );
		}
	}; // task_scheduler

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

			daw::move ( *task )( );
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
		static_assert( are_tasks_v<Tasks...>,
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
		static_assert( are_tasks_v<Tasks...>,
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
