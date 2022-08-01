// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _DELAYED_SCHEDULER_H
#define _DELAYED_SCHEDULER_H

#include <queue>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"

// Implements scheduling actions at specific times.
//
// Actions can be of an arbitrary type `T` that is copyable or movable.
// By default the type is `absl::AnyInvocable<void() &&>` which represents
// callback that can be run at most once.
//
// By design the class doesn't implement execution of scheduled actions.
// Once their respective times are reached, actions become available through
// `AwaitNextOrStop()` to be executed at the caller's discretion.
//
// Thread-safe.
template <typename T = absl::AnyInvocable<void() &&>>
class DelayedScheduler {
 public:
  DelayedScheduler() noexcept : queue_pushed_(false), running_(true) {}

  // Schedules `callback` to be run at time `at`.
  // Returns `true` if the callback was scheduled.
  // Returns `false` if this scheduler has been stopped, in which case
  // `callback` is immediately deleted.
  bool ScheduleAt(absl::Time at,
                  T callback) noexcept ABSL_LOCKS_EXCLUDED(lock_) {
    absl::MutexLock mutex(&lock_);
    if (running_) {
      queue_.emplace(at, std::move(callback));
      queue_pushed_ = true;
    }
    return running_;
  }

  // Same as `ScheduleAt`, but schedules `callback` to be run `after` from now.
  bool ScheduleAfter(absl::Duration after,
                     T callback) noexcept ABSL_LOCKS_EXCLUDED(lock_) {
    return ScheduleAt(absl::Now() + after, std::move(callback));
  }

  // Stops the scheduler. Once stopped, it won't schedule any further actions.
  // Any pending or future call to `AwaitNextOrStop` will clear the queue of
  // waiting actions and return `nullopt`.
  void Stop() noexcept ABSL_LOCKS_EXCLUDED(lock_) {
    absl::MutexLock mutex(&lock_);
    running_ = false;
  }

  // Waits until time reaches the action with the least scheduled time and
  // returns it. Any actions with scheduled time already in the past are
  // returned immediately one by on.
  //
  // If scheduler has been `Stop()`-ed, returns immediately with `nullopt`.
  // Commonly the caller will run this function within a dedicated thread in a
  // loop similar to this one:
  //
  //   absl::optional<Action> action;
  //   while ((action = scheduler.AwaitNextOrStop()).has_value()) {
  //     // Run `action`.
  //   }
  //   // Exit the thread.
  absl::optional<T> AwaitNextOrStop() noexcept ABSL_LOCKS_EXCLUDED(lock_) {
    absl::MutexLock mutex(&lock_);
    while (running_) {
      const absl::Duration remaining = queue_.empty()
                                           ? absl::InfiniteDuration()
                                           : (queue_.top().at - absl::Now());
      if (remaining <= absl::ZeroDuration()) {
        absl::optional<T> ready(
            std::move(const_cast<Entry&>(queue_.top()).callback));
        queue_.pop();
        return ready;
      }
      lock_.AwaitWithTimeout(
          absl::Condition(this, &DelayedScheduler::PushedOrStopped), remaining);
      queue_pushed_ = false;
    }
    queue_ = std::priority_queue<Entry>();
    return absl::nullopt;
  }

 private:
  struct Entry {
    Entry(absl::Time at_, T callback_) noexcept
        : at(at_), callback(std::move(callback_)) {}

    // Descending order by `at`.
    bool operator<(const Entry& other) const noexcept { return at > other.at; }

    absl::Time at;
    T callback;
  };

  bool PushedOrStopped() noexcept { return queue_pushed_ || !running_; }

  absl::Mutex lock_;
  std::priority_queue<Entry> queue_ ABSL_GUARDED_BY(lock_);
  bool queue_pushed_ ABSL_GUARDED_BY(lock_);
  bool running_ ABSL_GUARDED_BY(lock_);
};

#if __cplusplus >= 201703L
// https://en.cppreference.com/w/cpp/language/class_template_argument_deduction
DelayedScheduler()->DelayedScheduler<>;
#endif

#endif  // _DELAYED_SCHEDULER_H
