# DPDK: link the virtio poll-mode driver

## Problem

Seastar builds DPDK as a collection of static libraries, but its explicit
library list does not include `librte_net_virtio`. As a result, a virtio
network device can be correctly bound to a DPDK-compatible kernel driver while
the Seastar executable still has no driver capable of probing it.

On a Google Cloud VM with two Red Hat virtio network devices, the second device
was bound to `uio_pci_generic`, but a DPDK-enabled Seastar application exited
with:

```text
EAL: Error - exiting with code: 1
  Cause: No Ethernet ports - bye
```

This reproduces #336.

## Solution

Add `net_virtio` to the DPDK libraries collected by `Finddpdk.cmake`. This
causes the virtio poll-mode driver and its static registration data to be
included in Seastar's DPDK object and final library.

Closes #336.

## Changes

* `cmake/Finddpdk.cmake` — add the `net_virtio` poll-mode driver to the list of
  statically linked DPDK libraries.

## Testing

The change was tested on the same VM, with the same compiler, DPDK build,
hugepage configuration, application, command line, and virtio NIC. The test
removed and restored only the `net_virtio` entry in `Finddpdk.cmake`, rebuilding
`demos/tcp_demo` after each change.

Without the change:

* `libseastar.so` contained no `net_virtio_pmd_info` or
  `rte_virtio_net_pci_pmd` symbols.
* DPDK did not probe the bound virtio NIC.
* The application exited with `Cause: No Ethernet ports - bye`.

With the change:

* `libseastar.so` contained `net_virtio_pmd_info`,
  `rte_virtio_net_pci_pmd`, and `rte_virtio_net_pci_pmd_init`.
* DPDK logged `Probe PCI driver: net_virtio` for the bound NIC.
* Seastar reported `ports number: 1`, demonstrating that the original failure
  was resolved.

The VM's virtio device subsequently exposed a separate, pre-existing checksum
capability incompatibility in `dpdk_device::init_port_start()`. That assertion
occurs after successful driver probing and port discovery and is unrelated to
the missing static virtio PMD addressed here.
