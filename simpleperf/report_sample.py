#!/usr/bin/env python3
#
# Copyright (C) 2016 The Android Open Source Project
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

"""report_sample.py: report samples in the same format as `perf script`.
"""

from simpleperf_report_lib import ReportLib
from simpleperf_utils import BaseArgumentParser, flatten_arg_list
from typing import List, Set, Optional


def report_sample(
        record_file: str,
        symfs_dir: str,
        kallsyms_file: str,
        show_tracing_data: bool,
        proguard_mapping_file: List[str],
        header: bool,
        comm_filter: Set[str],
        trace_offcpu: Optional[str],
        sample_filter: Optional[str]):
    """ read record_file, and print each sample"""
    lib = ReportLib()

    lib.ShowIpForUnknownSymbol()
    for file_path in proguard_mapping_file:
        lib.AddProguardMappingFile(file_path)
    if symfs_dir is not None:
        lib.SetSymfs(symfs_dir)
    if record_file is not None:
        lib.SetRecordFile(record_file)
    if kallsyms_file is not None:
        lib.SetKallsymsFile(kallsyms_file)
    if trace_offcpu:
        lib.SetTraceOffCpuMode(trace_offcpu)
    if sample_filter:
        lib.SetSampleFilter(sample_filter)

    if header:
        print("# ========")
        print("# cmdline : %s" % lib.GetRecordCmd())
        print("# arch : %s" % lib.GetArch())
        for k, v in lib.MetaInfo().items():
            print('# %s : %s' % (k, v.replace('\n', ' ')))
        print("# ========")
        print("#")

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

        sec = sample.time // 1000000000
        usec = (sample.time - sec * 1000000000) // 1000
        print('%s\t%d/%d [%03d] %d.%06d: %d %s:' % (sample.thread_comm,
                                                    sample.pid, sample.tid, sample.cpu, sec,
                                                    usec, sample.period, event.name))
        print('\t%16x %s (%s)' % (sample.ip, symbol.symbol_name, symbol.dso_name))
        for i in range(callchain.nr):
            entry = callchain.entries[i]
            print('\t%16x %s (%s)' % (entry.ip, entry.symbol.symbol_name, entry.symbol.dso_name))
        if show_tracing_data:
            data = lib.GetTracingDataOfCurrentSample()
            if data:
                print('\ttracing data:')
                for key, value in data.items():
                    print('\t\t%s : %s' % (key, value))
        print('')


def main():
    parser = BaseArgumentParser(description='Report samples in perf.data.')
    parser.add_argument('--symfs',
                        help='Set the path to find binaries with symbols and debug info.')
    parser.add_argument('--kallsyms', help='Set the path to find kernel symbols.')
    parser.add_argument('-i', '--record_file', nargs='?', default='perf.data',
                        help='Default is perf.data.')
    parser.add_argument('--show_tracing_data', action='store_true', help='print tracing data.')
    parser.add_argument(
        '--proguard-mapping-file', nargs='+',
        help='Add proguard mapping file to de-obfuscate symbols',
        default=[])
    parser.add_argument('--header', action='store_true',
                        help='Show metadata header, like perf script --header')
    parser.add_argument('--comm', nargs='+', action='append', help="""
        Use samples only in threads with selected names.""")
    parser.add_trace_offcpu_option()
    parser.add_sample_filter_options()
    args = parser.parse_args()
    report_sample(
        record_file=args.record_file,
        symfs_dir=args.symfs,
        kallsyms_file=args.kallsyms,
        show_tracing_data=args.show_tracing_data,
        proguard_mapping_file=args.proguard_mapping_file,
        header=args.header,
        comm_filter=set(flatten_arg_list(args.comm)),
        trace_offcpu=args.trace_offcpu,
        sample_filter=args.sample_filter)


if __name__ == '__main__':
    main()
