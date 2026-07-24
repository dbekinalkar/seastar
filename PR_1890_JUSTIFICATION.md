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

### Test environment

* Ubuntu 22.04
* Linux 6.8.0-1064-gcp
* DPDK 23.07
* Red Hat virtio NIC (`1af4:1000`) bound to `uio_pci_generic`
* One Seastar shard
* Reactor backends tested: `io_uring`, `linux-aio`, and `epoll`

The same DPDK-enabled `tcp_demo` was run with each reactor backend.

Before the fix:

| Reactor backend | Result |
| --- | --- |
| `epoll` | `Sleeping... Done.`; exit status 0 |
| `linux-aio` | `Sleeping... Done.`; exit status 0 |
| `io_uring` | Hung after `Sleeping...`; killed by timeout |

After the fix:

| Reactor backend | Result |
| --- | --- |
| `io_uring` | `Sleeping... Done.`; exit status 0 |

The test used:

```console
sudo build/dev/demos/tcp_demo \
  --reactor-backend <backend> \
  --network-stack native \
  --dpdk-pmd \
  --dhcp 1 \
  --smp 1 \
  --memory 512M \
  --lro off
```

A unit regression test was also added to verify that `sleep()` completes while
the reactor remains active processing I/O.
