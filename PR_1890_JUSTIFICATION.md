# reactor: rearm the io_uring high-resolution timer

## Problem

When Seastar uses the io_uring reactor with DPDK, `seastar::sleep()` may never
complete. DPDK keeps the reactor active, so it does not enter the blocking path
where the high-resolution timer completion is normally armed.

As a result, the timerfd expiration is not observed and the application hangs
after printing `Sleeping...`.

Closes #1890.

## Solution

Rearm the high-resolution timer completion from `kernel_submit_work()` before
submitting pending io_uring operations. This ensures timer expirations are
monitored during active processing as well as while the reactor is idle.

## Changes

* `src/core/reactor_backend.cc` — call
  `_hrtimer_completion.maybe_rearm(*this)` before `io_uring_submit()`.
* `tests/unit/reactor_backend_test.cc` — add a regression test that verifies a
  sleep completes while the reactor is processing I/O.

## Testing

The issue was reproduced with the io_uring reactor and DPDK: the test
application printed `Sleeping...` but the sleep future never completed.

With the fix applied, the same application, reactor backend, DPDK
configuration, and network device complete successfully and print:

```text
Sleeping... Done.
```
