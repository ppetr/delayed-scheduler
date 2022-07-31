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

template <typename T = absl::AnyInvocable<void() &&>>
class DelayedScheduler {
 public:
  DelayedScheduler() : queue_pushed_(false), running_(true) {}

  bool ScheduleAt(absl::Time at, T callback) ABSL_LOCKS_EXCLUDED(lock_) {
    absl::MutexLock mutex(&lock_);
    if (running_) {
      queue_.emplace(at, std::move(callback));
      queue_pushed_ = true;
    }
    return running_;
  }

  bool ScheduleAfter(absl::Duration after, T callback)
      ABSL_LOCKS_EXCLUDED(lock_) {
    return ScheduleAt(absl::Now() + after, std::move(callback));
  }

  void Stop() ABSL_LOCKS_EXCLUDED(lock_) {
    absl::MutexLock mutex(&lock_);
    running_ = false;
  }

  absl::optional<T> AwaitNextOrStop() ABSL_LOCKS_EXCLUDED(lock_) {
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
    Entry(absl::Time at_, T callback_)
        : at(at_), callback(std::move(callback_)) {}

    bool operator<(const Entry& other) const { return at > other.at; }

    absl::Time at;
    T callback;
  };

  bool PushedOrStopped() { return queue_pushed_ || !running_; }

  absl::Mutex lock_;
  std::priority_queue<Entry> queue_ ABSL_GUARDED_BY(lock_);
  bool queue_pushed_ ABSL_GUARDED_BY(lock_);
  bool running_ ABSL_GUARDED_BY(lock_);
};

#if __cplusplus >= 201703L
// https://en.cppreference.com/w/cpp/language/class_template_argument_deduction
DelayedScheduler() -> DelayedScheduler<>;
#endif

#endif  // _DELAYED_SCHEDULER_H
