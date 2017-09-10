// The MIT License (MIT)
//
// Copyright (c) 2016-2017 Darrell Wright
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <atomic>
#include <boost/optional.hpp>
#include <functional>
#include <list>
#include <memory>
#include <thread>
#include <vector>

#include <daw/daw_locked_stack.h>
#include <daw/daw_locked_value.h>
#include <daw/daw_semaphore.h>
#include <daw/daw_utility.h>

namespace daw {
	using task_t = std::function<void( )>;

	namespace impl {
		class task_scheduler_impl;

		void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself,
		                  boost::optional<daw::shared_semaphore> semaphore );

		void task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself );

		class task_scheduler_impl : public std::enable_shared_from_this<task_scheduler_impl> {
			using task_queue_t = daw::locked_stack_t<daw::task_t>;
			daw::lockable_value_t<std::vector<std::thread>> m_threads;
			std::vector<task_queue_t> m_tasks;
			std::atomic_bool m_continue;
			bool m_block_on_destruction;
			size_t const m_num_threads;
			std::atomic<size_t> m_task_count;
			daw::lockable_value_t<std::list<boost::optional<std::thread>>> m_other_threads;

			std::weak_ptr<task_scheduler_impl> get_weak_this( );

			friend void impl::task_runner( size_t id, std::weak_ptr<task_scheduler_impl> wself,
			                               boost::optional<daw::shared_semaphore> semaphore );

		  public:
			task_scheduler_impl( std::size_t num_threads, bool block_on_destruction );
			~task_scheduler_impl( );
			task_scheduler_impl( task_scheduler_impl && ) = delete; // TODO: investigate why implicitly deleted
			task_scheduler_impl &
			operator=( task_scheduler_impl && ) = delete; // TODO: investigate why implicitly deleted

			task_scheduler_impl( task_scheduler_impl const & ) = delete;
			task_scheduler_impl &operator=( task_scheduler_impl const & ) = delete;

			template<typename Task>
			void add_task( Task && task ) noexcept {
				auto id = ( m_task_count++ ) % m_num_threads;
				m_tasks[id].emplace_back( std::forward<Task>( task ) );
			}

			void start( );
			void stop( bool block = true ) noexcept;
			bool started( ) const;
			size_t size( ) const {
				return m_tasks.size( );
			}
			bool am_i_in_pool( ) const noexcept;

			daw::shared_semaphore start_temp_task_runners( size_t task_count = 1 );
		}; // task_scheduler_impl

	} // namespace impl

} // namespace daw

