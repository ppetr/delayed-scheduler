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

use std::cmp::Ordering;
use std::collections::BinaryHeap;
use std::option::Option;
use std::sync::{Condvar, Mutex};
use std::time::{Duration, Instant};
use std::vec::Vec;

struct Entry<T> {
    at: Instant,
    position: i64,
    callback: T,
}

impl<T> PartialEq for Entry<T> {
    fn eq(&self, other: &Self) -> bool {
        (self.at, self.position) == (other.at, other.position)
    }
}

impl<T> Eq for Entry<T> {}

impl<T> Ord for Entry<T> {
    fn cmp(&self, other: &Self) -> Ordering {
        (other.at, self.position).cmp(&(self.at, other.position))
    }
}

impl<T> PartialOrd for Entry<T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

pub struct Poisoned<T> {
    pub drained_callbacks: Vec<(Instant, T)>,
}

struct Queue<T> {
    heap: BinaryHeap<Entry<T>>,
    position: i64,
}

pub struct DelayedScheduler<T> {
    mutex: Mutex<Queue<T>>,
    pushed: Condvar,
}

impl<T> DelayedScheduler<T> {
    fn new() -> Self {
        DelayedScheduler::<T> {
            mutex: Mutex::<Queue<T>>::new(Queue::<T> {
                heap: BinaryHeap::<Entry<T>>::new(),
                position: 0,
            }),
            pushed: Condvar::new(),
        }
    }

    fn schedule_at(&self, at: Instant, callback: T) {
        // We don't expect any thread to panic under `self.mutex`, therefore locking should always
        // succeed.
        let mut queue = self.mutex.lock().unwrap();
        let position = queue.position;
        queue.position += 1;
        queue.heap.push(Entry::<T> {
            at: at,
            position: position,
            callback: callback,
        });
        self.pushed.notify_all();
    }

    fn schedule_after(&self, after: Duration, callback: T) {
        self.schedule_at(Instant::now() + after, callback)
    }

    fn await_next(&self) -> T {
        let mut queue = self.mutex.lock().unwrap();
        loop {
            let duration = match queue.heap.peek() {
                Some(entry) => entry.at.saturating_duration_since(Instant::now()),
                _ => Duration::MAX,
            };
            if duration.is_zero() {
                return queue.heap.pop().unwrap().callback;
            }
            queue = self.pushed.wait_timeout(queue, duration).unwrap().0;
        }
    }

    fn drain(self) -> Option<Vec<(Instant, T)>> {
        let mut heap = self.mutex.into_inner().ok()?.heap;
        let mut abandoned = Vec::<(Instant, T)>::with_capacity(heap.len());
        while let Some(entry) = heap.pop() {
            abandoned.push((entry.at, entry.callback))
        }
        Some(abandoned)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn drained_items<T>(scheduler: DelayedScheduler<T>) -> Vec<T> {
        scheduler
            .drain()
            .unwrap()
            .into_iter()
            .map(|x| x.1)
            .collect()
    }

    #[test]
    fn schedules_in_the_past() {
        let scheduler = DelayedScheduler::<i32>::new();
        scheduler.schedule_after(Duration::ZERO, -1);
        assert_eq!(scheduler.await_next(), -1);
        assert_eq!(drained_items(scheduler), vec![]);
    }

    #[test]
    fn orders_elements() {
        let scheduler = DelayedScheduler::<i32>::new();
        scheduler.schedule_after(Duration::from_millis(1), 1);
        scheduler.schedule_after(Duration::from_millis(3), 3);
        scheduler.schedule_after(Duration::from_millis(2), 2);
        assert_eq!(scheduler.await_next(), 1);
        assert_eq!(scheduler.await_next(), 2);
        assert_eq!(scheduler.await_next(), 3);
        assert_eq!(drained_items(scheduler), vec![]);
    }

    #[test]
    fn drains_elements() {
        let scheduler = DelayedScheduler::<i32>::new();
        scheduler.schedule_after(Duration::from_millis(1), 1);
        scheduler.schedule_after(Duration::from_millis(3), 3);
        scheduler.schedule_after(Duration::from_millis(2), 2);
        assert_eq!(drained_items(scheduler), vec![1, 2, 3]);
    }
}
