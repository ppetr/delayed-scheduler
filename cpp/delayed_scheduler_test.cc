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

#include "delayed_scheduler.h"

#include "absl/memory/memory.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

TEST(DelayedSchedulerTest, Empty) {
  DelayedScheduler<> scheduler;
  scheduler.Stop();
  EXPECT_EQ(scheduler.AwaitNextOrStop(), absl::nullopt);
}

#if __cplusplus >= 201703L
TEST(DelayedSchedulerTest, DeducedTemplateTypeCpp17) {
  DelayedScheduler scheduler;
  // Move-only lambda.
  scheduler.ScheduleAfter(absl::ZeroDuration(),
                          [ptr = absl::make_unique<int>()]() {});
}
#endif

TEST(DelayedSchedulerTest, SchedulesInThePast) {
  DelayedScheduler<int> scheduler;
  scheduler.ScheduleAfter(absl::Hours(-1), -1);
  EXPECT_EQ(scheduler.AwaitNextOrStop(), -1);
  scheduler.Stop();
  EXPECT_EQ(scheduler.AwaitNextOrStop(), absl::nullopt);
}

TEST(DelayedSchedulerTest, OrdersElements) {
  DelayedScheduler<int> scheduler;
  const absl::Time now = absl::Now();
  scheduler.ScheduleAt(now + absl::Milliseconds(1), 1);
  scheduler.ScheduleAt(now + absl::Milliseconds(3), 3);
  scheduler.ScheduleAt(now + absl::Milliseconds(2), 2);
  EXPECT_EQ(scheduler.AwaitNextOrStop(), 1);
  EXPECT_EQ(scheduler.AwaitNextOrStop(), 2);
  EXPECT_EQ(scheduler.AwaitNextOrStop(), 3);
  scheduler.Stop();
  EXPECT_EQ(scheduler.AwaitNextOrStop(), absl::nullopt);
}

// A move-only class that invokes a given function in its destructor (unless
// moved out).
struct MockDestructor {
  MockDestructor(absl::AnyInvocable<void()> destructor)
      : destructor_(std::move(destructor)) {}
  MockDestructor(MockDestructor&&) = default;
  MockDestructor& operator=(MockDestructor&& other) {
    destructor_ = absl::exchange(other.destructor_, nullptr);
    return *this;
  }
  ~MockDestructor() {
    if (destructor_) {
      std::move(destructor_)();
    }
  }

  absl::AnyInvocable<void()> destructor_;
};

TEST(DelayedSchedulerTest, StopEmptiesQueueOnNextAwait) {
  testing::MockFunction<void()> destructor;
  DelayedScheduler<MockDestructor> scheduler;
  EXPECT_TRUE(scheduler.ScheduleAfter(
      absl::ZeroDuration(), MockDestructor(destructor.AsStdFunction())));
  scheduler.Stop();
  EXPECT_CALL(destructor, Call());
  ASSERT_EQ(scheduler.AwaitNextOrStop(), absl::nullopt);
  EXPECT_CALL(destructor, Call()).Times(0);
}

TEST(DelayedSchedulerTest, DoesNotScheduleAfterStop) {
  testing::MockFunction<void()> destructor;
  DelayedScheduler<MockDestructor> scheduler;
  scheduler.Stop();
  EXPECT_CALL(destructor, Call());
  EXPECT_FALSE(scheduler.ScheduleAfter(
      absl::ZeroDuration(), MockDestructor(destructor.AsStdFunction())));
  EXPECT_CALL(destructor, Call()).Times(0);
  EXPECT_EQ(scheduler.AwaitNextOrStop(), absl::nullopt);
}

TEST(DelayedSchedulerTest, NeverAwaited) {
  testing::MockFunction<void()> destructor;
  DelayedScheduler<MockDestructor> scheduler;
  EXPECT_TRUE(scheduler.ScheduleAfter(
      absl::ZeroDuration(), MockDestructor(destructor.AsStdFunction())));
  scheduler.Stop();
  EXPECT_CALL(destructor, Call());
}

}  // namespace
