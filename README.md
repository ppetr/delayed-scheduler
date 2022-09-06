# Delayed Scheduler

Implements scheduling actions at specific times. Waiting for actions can be
performed by a single, dedicated thread.

By design this module doesn't implement execution of scheduled actions. Once
their respective times are reached, actions become available to be executed at
the caller's discretion.
