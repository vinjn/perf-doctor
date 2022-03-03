#!/usr/bin/env python3
#
# Copyright (C) 2021 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""stackcollapse.py: convert perf.data to Brendan Gregg's "Folded Stacks" format,
    which can be read by https://github.com/brendangregg/FlameGraph, and many
    other tools.

  Example:
    ./app_profiler.py
    ./stackcollapse.py | ~/FlameGraph/flamegraph.pl --color=java --countname=ns > flamegraph.svg
"""

from collections import defaultdict
from simpleperf_report_lib import ReportLib
from simpleperf_utils import BaseArgumentParser, flatten_arg_list
from typing import DefaultDict, List, Optional, Set

import logging
import sys


def collapse_stacks(
        record_file: str,
        symfs_dir: str,
        kallsyms_file: str,
        proguard_mapping_file: List[str],
        event_filter: str,
        include_pid: bool,
        include_tid: bool,
        annotate_kernel: bool,
        annotate_jit: bool,
        include_addrs: bool,
        comm_filter: Set[str],
        sample_filter: Optional[str]):
    """read record_file, aggregate per-stack and print totals per-stack"""
    lib = ReportLib()

    if include_addrs:
        lib.ShowIpForUnknownSymbol()
    for file_path in proguard_mapping_file:
        lib.AddProguardMappingFile(file_path)
    if symfs_dir is not None:
        lib.SetSymfs(symfs_dir)
    if record_file is not None:
        lib.SetRecordFile(record_file)
    if kallsyms_file is not None:
        lib.SetKallsymsFile(kallsyms_file)
    if sample_filter:
        lib.SetSampleFilter(sample_filter)

    stacks: DefaultDict[str, int] = defaultdict(int)
    event_defaulted = False
    event_warning_shown = False
    while True:
        sample = lib.GetNextSample()
        if sample is None:
            lib.Close()
            break
        if comm_filter:
            if sample.thread_comm not in comm_filter:
                continue
        event = lib.GetEventOfCurrentSample()
        symbol = lib.GetSymbolOfCurrentSample()
        callchain = lib.GetCallChainOfCurrentSample()
        if not event_filter:
            event_filter = event.name
            event_defaulted = True
        elif event.name != event_filter:
            if event_defaulted and not event_warning_shown:
                logging.warning(
                    'Input has multiple event types. Filtering for the first event type seen: %s' %
                    event_filter)
                event_warning_shown = True
            continue

        stack = []
        for i in range(callchain.nr):
            entry = callchain.entries[i]
            func = entry.symbol.symbol_name
            if annotate_kernel and "kallsyms" in entry.symbol.dso_name or ".ko" in entry.symbol.dso_name:
                func += '_[k]'  # kernel
            if annotate_jit and entry.symbol.dso_name == "[JIT app cache]":
                func += '_[j]'  # jit
            stack.append(func)
        if include_tid:
            stack.append("%s-%d/%d" % (sample.thread_comm, sample.pid, sample.tid))
        elif include_pid:
            stack.append("%s-%d" % (sample.thread_comm, sample.pid))
        else:
            stack.append(sample.thread_comm)
        stack.reverse()
        stacks[";".join(stack)] += sample.period

    for k in sorted(stacks.keys()):
        print("%s %d" % (k, stacks[k]))


def main():
    parser = BaseArgumentParser(description=__doc__)
    parser.add_argument('--symfs',
                        help='Set the path to find binaries with symbols and debug info.')
    parser.add_argument('--kallsyms', help='Set the path to find kernel symbols.')
    parser.add_argument('-i', '--record_file', nargs='?', default='perf.data',
                        help='Default is perf.data.')
    parser.add_argument('--pid', action='store_true', help='Include PID with process names')
    parser.add_argument('--tid', action='store_true', help='Include TID and PID with process names')
    parser.add_argument('--kernel', action='store_true',
                        help='Annotate kernel functions with a _[k]')
    parser.add_argument('--jit', action='store_true', help='Annotate JIT functions with a _[j]')
    parser.add_argument('--addrs', action='store_true',
                        help='include raw addresses where symbols can\'t be found')
    parser.add_argument(
        '--proguard-mapping-file', nargs='+',
        help='Add proguard mapping file to de-obfuscate symbols',
        default=[])
    sample_filter_group = parser.add_argument_group('Sample filter options')
    parser.add_sample_filter_options(sample_filter_group, False)
    sample_filter_group.add_argument('--event-filter', nargs='?', default='',
                                     help='Event type filter e.g. "cpu-cycles" or "instructions"')
    sample_filter_group.add_argument('--comm', nargs='+', action='append', help="""
      Use samples only in threads with selected names.""")
    args = parser.parse_args()
    collapse_stacks(
        record_file=args.record_file,
        symfs_dir=args.symfs,
        kallsyms_file=args.kallsyms,
        proguard_mapping_file=args.proguard_mapping_file,
        event_filter=args.event_filter,
        include_pid=args.pid,
        include_tid=args.tid,
        annotate_kernel=args.kernel,
        annotate_jit=args.jit,
        include_addrs=args.addrs,
        comm_filter=set(flatten_arg_list(args.comm)),
        sample_filter=args.sample_filter)


if __name__ == '__main__':
    main()
