# Collect ETM data for AutoFDO

[TOC]

## Introduction

ETM is a hardware feature available on arm64 devices. It collects the instruction stream running on
each cpu. ARM uses ETM as an alternative for LBR (last branch record) on x86.
Simpleperf supports collecting ETM data, and converting it to input files for AutoFDO, which can
then be used for PGO (profile-guided optimization) during compilation.

On ARMv8, ETM is considered as an external debug interface (unless ARMv8.4 Self-hosted Trace
extension is impelemented). So it needs to be enabled explicitly in the bootloader, and isn't
available on user devices. For Pixel devices, it's available on EVT and DVT devices on Pixel 4,
Pixel 4a (5G) and Pixel 5. To test if it's available on other devices, you can follow commands in
this doc and see if you can record any ETM data.

## Examples

Below are examples collecting ETM data for AutoFDO. It has two steps: first recording ETM data,
second converting ETM data to AutoFDO input files.

Record ETM data:

```sh
# preparation: we need to be root to record ETM data
$ adb root
$ adb shell
redfin:/ \# cd data/local/tmp
redfin:/data/local/tmp \#

# Do a system wide collection, it writes output to perf.data.
# If only want ETM data for kernel, use `-e cs-etm:k`.
# If only want ETM data for userspace, use `-e cs-etm:u`.
redfin:/data/local/tmp \# simpleperf record -e cs-etm --duration 3 -a

# To reduce file size and time converting to AutoFDO input files, we recommend converting ETM data
# into an intermediate branch-list format.
redfin:/data/local/tmp \# simpleperf inject --output branch-list -o branch_list.data
```

Converting ETM data to AutoFDO input files needs to read binaries.
So for userspace libraries, they can be converted on device. For kernel, it needs
to be converted on host, with vmlinux and kernel modules available.

Convert ETM data for userspace libraries:

```sh
# Injecting ETM data on device. It writes output to perf_inject.data.
# perf_inject.data is a text file, containing branch counts for each library.
redfin:/data/local/tmp \# simpleperf inject -i branch_list.data
```

Convert ETM data for kernel:

```sh
# pull ETM data to host.
host $ adb pull /data/local/tmp/branch_list.data
# download vmlinux and kernel modules to <binary_dir>
# host simpleperf is in <aosp-top>/system/extras/simpleperf/scripts/bin/linux/x86_64/simpleperf,
# or you can build simpleperf by `mmma system/extras/simpleperf`.
host $ simpleperf inject --symdir <binary_dir> -i branch_list.data
```

The generated perf_inject.data may contain branch info for multiple binaries. But AutoFDO only
accepts one at a time. So we need to split perf_inject.data.
The format of perf_inject.data is below:

```perf_inject.data format

executed range with count info for binary1
branch with count info for binary1
// name for binary1

executed range with count info for binary2
branch with count info for binary2
// name for binary2

...
```

We need to split perf_inject.data, and make sure one file only contains info for one binary.

Then we can use [AutoFDO](https://github.com/google/autofdo) to create profile like below:

```sh
# perf_inject_kernel.data is split from perf_inject.data, and only contains branch info for [kernel.kallsyms].
host $ autofdo/create_llvm_prof -profile perf_inject_kernel.data -profiler text -binary vmlinux -out a.prof -format binary
```

Then we can use a.prof for PGO during compilation, via `-fprofile-sample-use=a.prof`.
[Here](https://clang.llvm.org/docs/UsersManual.html#using-sampling-profilers) are more details.

## Collect ETM data with a daemon

Android also has a daemon collecting ETM data periodically. It only runs on userdebug and eng
devices. The source code is in `<aosp-top>/system/extras/profcollectd`.

## Support ETM in the kernel

To let simpleperf use ETM function, we need to enable Coresight driver in the kernel, which lives in
`<linux_kernel>/drivers/hwtracing/coresight`.

The Coresight driver can be enabled by below kernel configs:

```config
	CONFIG_CORESIGHT=y
	CONFIG_CORESIGHT_LINK_AND_SINK_TMC=y
	CONFIG_CORESIGHT_SOURCE_ETM4X=y
	CONFIG_CORESIGHT_DYNAMIC_REPLICATOR=y
```

On Kernel 5.10+, we can build Coresight driver as kernel modules instead.

Android common kernel 5.10+ should have all the Coresight patches needed. And we have backported
necessary Coresight patches to Android common kernel 4.14 and 4.19. Android common kernel 5.4
misses a few patches. Please create an [ndk issue](https://github.com/android/ndk/issues) if you
need ETM function on 5.4 kernel.

Besides Coresight driver, we also need to add Coresight devices in device tree. An example is in
https://github.com/torvalds/linux/blob/master/arch/arm64/boot/dts/arm/juno-base.dtsi. There should
be a path flowing ETM data from ETM device through funnels, ETF and replicators, all the way to
ETR, which writes ETM data to system memory.

## Enable ETM in the bootloader

Unless ARMv8.4 Self-hosted Trace extension is implemented, ETM is considered as an external debug
interface. It may be disabled by fuse (like JTAG). So we need to check if ETM is disabled, and
if bootloader provides a way to reenable it.

We can tell if ETM is disable by checking its TRCAUTHSTATUS register, which is exposed in sysfs,
like /sys/bus/coresight/devices/coresight-etm0/mgmt/trcauthstatus. To reenable ETM, we need to
enable non-Secure non-invasive debug on ARM CPU. The method depends on chip vendors(SOCs).


## Related docs

* [Arm Architecture Reference Manual Armv8, D3 AArch64 Self-hosted Trace](https://developer.arm.com/documentation/ddi0487/latest)
* [ARM ETM Architecture Specification](https://developer.arm.com/documentation/ihi0064/latest/)
* [ARM CoreSight Architecture Specification](https://developer.arm.com/documentation/ihi0029/latest)
* [CoreSight Components Technical Reference Manual](https://developer.arm.com/documentation/ddi0314/h/)
* [CoreSight Trace Memory Controller Technical Reference Manual](https://developer.arm.com/documentation/ddi0461/b/)
* [OpenCSD library for decoding ETM data](https://github.com/Linaro/OpenCSD)
* [AutoFDO tool for converting profile data](https://github.com/google/autofdo)
