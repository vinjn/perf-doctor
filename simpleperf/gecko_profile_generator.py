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

"""gecko_profile_generator.py: converts perf.data to Gecko Profile Format,
    which can be read by https://profiler.firefox.com/.

  Example:
    ./app_profiler.py
    ./gecko_profile_generator.py | gzip > gecko-profile.json.gz

  Then open gecko-profile.json.gz in https://profiler.firefox.com/
"""

import json
import sys

from dataclasses import dataclass, field
from simpleperf_report_lib import ReportLib
from simpleperf_utils import BaseArgumentParser, flatten_arg_list
from typing import List, Dict, Optional, NamedTuple, Set, Tuple


StringID = int
StackID = int
FrameID = int
CategoryID = int
Milliseconds = float
GeckoProfile = Dict


# https://github.com/firefox-devtools/profiler/blob/53970305b51b9b472e26d7457fee1d66cd4e2737/src/types/gecko-profile.js#L156
class Frame(NamedTuple):
    string_id: StringID
    relevantForJS: bool
    innerWindowID: int
    implementation: None
    optimizations: None
    line: None
    column: None
    category: CategoryID
    subcategory: int


# https://github.com/firefox-devtools/profiler/blob/53970305b51b9b472e26d7457fee1d66cd4e2737/src/types/gecko-profile.js#L216
class Stack(NamedTuple):
    prefix_id: Optional[StackID]
    frame_id: FrameID
    category_id: CategoryID


# https://github.com/firefox-devtools/profiler/blob/53970305b51b9b472e26d7457fee1d66cd4e2737/src/types/gecko-profile.js#L90
class Sample(NamedTuple):
    stack_id: Optional[StackID]
    time_ms: Milliseconds
    responsiveness: int


# Schema: https://github.com/firefox-devtools/profiler/blob/53970305b51b9b472e26d7457fee1d66cd4e2737/src/types/profile.js#L425
# Colors must be defined in:
# https://github.com/firefox-devtools/profiler/blob/50124adbfa488adba6e2674a8f2618cf34b59cd2/res/css/categories.css
CATEGORIES = [
    {
        "name": 'User',
        # Follow Brendan Gregg's Flamegraph convention: yellow for userland
        # https://github.com/brendangregg/FlameGraph/blob/810687f180f3c4929b5d965f54817a5218c9d89b/flamegraph.pl#L419
        "color": 'yellow',
        "subcategories": ['Other']
    },
    {
        "name": 'Kernel',
        # Follow Brendan Gregg's Flamegraph convention: orange for kernel
        # https://github.com/brendangregg/FlameGraph/blob/810687f180f3c4929b5d965f54817a5218c9d89b/flamegraph.pl#L417
        "color": 'orange',
        "subcategories": ['Other']
    },
    {
        "name": 'Native',
        # Follow Brendan Gregg's Flamegraph convention: yellow for userland
        # https://github.com/brendangregg/FlameGraph/blob/810687f180f3c4929b5d965f54817a5218c9d89b/flamegraph.pl#L419
        "color": 'yellow',
        "subcategories": ['Other']
    },
    {
        "name": 'DEX',
        # Follow Brendan Gregg's Flamegraph convention: green for Java/JIT
        # https://github.com/brendangregg/FlameGraph/blob/810687f180f3c4929b5d965f54817a5218c9d89b/flamegraph.pl#L411
        "color": 'green',
        "subcategories": ['Other']
    },
    {
        "name": 'OAT',
        # Follow Brendan Gregg's Flamegraph convention: green for Java/JIT
        # https://github.com/brendangregg/FlameGraph/blob/810687f180f3c4929b5d965f54817a5218c9d89b/flamegraph.pl#L411
        "color": 'green',
        "subcategories": ['Other']
    },
    # Not used by this exporter yet, but some Firefox Profiler code assumes
    # there is an 'Other' category by searching for a category with
    # color=grey, so include this.
    {
        "name": 'Other',
        "color": 'grey',
        "subcategories": ['Other']
    },
]


@dataclass
class Thread:
    """A builder for a profile of a single thread.

    Attributes:
      comm: Thread command-line (name).
      pid: process ID of containing process.
      tid: thread ID.
      samples: Timeline of profile samples.
      frameTable: interned stack frame ID -> stack frame.
      stringTable: interned string ID -> string.
      stringMap: interned string -> string ID.
      stackTable: interned stack ID -> stack.
      stackMap: (stack prefix ID, leaf stack frame ID) -> interned Stack ID.
      frameMap: Stack Frame string -> interned Frame ID.
    """
    comm: str
    pid: int
    tid: int
    samples: List[Sample] = field(default_factory=list)
    frameTable: List[Frame] = field(default_factory=list)
    stringTable: List[str] = field(default_factory=list)
    # TODO: this is redundant with frameTable, could we remove this?
    stringMap: Dict[str, int] = field(default_factory=dict)
    stackTable: List[Stack] = field(default_factory=list)
    stackMap: Dict[Tuple[Optional[int], int], int] = field(default_factory=dict)
    frameMap: Dict[str, int] = field(default_factory=dict)

    def _intern_stack(self, frame_id: int, prefix_id: Optional[int]) -> int:
        """Gets a matching stack, or saves the new stack. Returns a Stack ID."""
        key = (prefix_id, frame_id)
        stack_id = self.stackMap.get(key)
        if stack_id is not None:
            return stack_id
        stack_id = len(self.stackTable)
        self.stackTable.append(Stack(prefix_id=prefix_id,
                                     frame_id=frame_id,
                                     category_id=0))
        self.stackMap[key] = stack_id
        return stack_id

    def _intern_string(self, string: str) -> int:
        """Gets a matching string, or saves the new string. Returns a String ID."""
        string_id = self.stringMap.get(string)
        if string_id is not None:
            return string_id
        string_id = len(self.stringTable)
        self.stringTable.append(string)
        self.stringMap[string] = string_id
        return string_id

    def _intern_frame(self, frame_str: str) -> int:
        """Gets a matching stack frame, or saves the new frame. Returns a Frame ID."""
        frame_id = self.frameMap.get(frame_str)
        if frame_id is not None:
            return frame_id
        frame_id = len(self.frameTable)
        self.frameMap[frame_str] = frame_id
        string_id = self._intern_string(frame_str)

        category = 0
        # Heuristic: kernel code contains "kallsyms" as the library name.
        if "kallsyms" in frame_str or ".ko" in frame_str:
            category = 1
        elif ".so" in frame_str:
            category = 2
        elif ".vdex" in frame_str:
            category = 3
        elif ".oat" in frame_str:
            category = 4

        self.frameTable.append(Frame(
            string_id=string_id,
            relevantForJS=False,
            innerWindowID=0,
            implementation=None,
            optimizations=None,
            line=None,
            column=None,
            category=category,
            subcategory=0,
        ))
        return frame_id

    def _add_sample(self, comm: str, stack: List[str], time_ms: Milliseconds) -> None:
        """Add a timestamped stack trace sample to the thread builder.

        Args:
          comm: command-line (name) of the thread at this sample
          stack: sampled stack frames. Root first, leaf last.
          time_ms: timestamp of sample in milliseconds
        """
        # Unix threads often don't set their name immediately upon creation.
        # Use the last name
        if self.comm != comm:
            self.comm = comm

        prefix_stack_id = None
        for frame in stack:
            frame_id = self._intern_frame(frame)
            prefix_stack_id = self._intern_stack(frame_id, prefix_stack_id)

        self.samples.append(Sample(stack_id=prefix_stack_id,
                                   time_ms=time_ms,
                                   responsiveness=0))

    def _to_json_dict(self) -> Dict:
        """Converts this Thread to GeckoThread JSON format."""
        # The samples aren't guaranteed to be in order. Sort them by time.
        self.samples.sort(key=lambda s: s.time_ms)

        # Gecko profile format is row-oriented data as List[List],
        # And a schema for interpreting each index.
        # Schema:
        # https://github.com/firefox-devtools/profiler/blob/main/docs-developer/gecko-profile-format.md
        # https://github.com/firefox-devtools/profiler/blob/53970305b51b9b472e26d7457fee1d66cd4e2737/src/types/gecko-profile.js#L230
        return {
            "tid": self.tid,
            "pid": self.pid,
            "name": self.comm,
            # https://github.com/firefox-devtools/profiler/blob/53970305b51b9b472e26d7457fee1d66cd4e2737/src/types/gecko-profile.js#L51
            "markers": {
                "schema": {
                    "name": 0,
                    "startTime": 1,
                    "endTime": 2,
                    "phase": 3,
                    "category": 4,
                    "data": 5,
                },
                "data": [],
            },
            # https://github.com/firefox-devtools/profiler/blob/53970305b51b9b472e26d7457fee1d66cd4e2737/src/types/gecko-profile.js#L90
            "samples": {
                "schema": {
                    "stack": 0,
                    "time": 1,
                    "responsiveness": 2,
                },
                "data": self.samples
            },
            # https://github.com/firefox-devtools/profiler/blob/53970305b51b9b472e26d7457fee1d66cd4e2737/src/types/gecko-profile.js#L156
            "frameTable": {
                "schema": {
                    "location": 0,
                    "relevantForJS": 1,
                    "innerWindowID": 2,
                    "implementation": 3,
                    "optimizations": 4,
                    "line": 5,
                    "column": 6,
                    "category": 7,
                    "subcategory": 8,
                },
                "data": self.frameTable,
            },
            # https://github.com/firefox-devtools/profiler/blob/53970305b51b9b472e26d7457fee1d66cd4e2737/src/types/gecko-profile.js#L216
            "stackTable": {
                "schema": {
                    "prefix": 0,
                    "frame": 1,
                    "category": 2,
                },
                "data": self.stackTable,
            },
            "stringTable": self.stringTable,
            "registerTime": 0,
            "unregisterTime": None,
            "processType": "default",
        }


def _gecko_profile(
        record_file: str,
        symfs_dir: Optional[str],
        kallsyms_file: Optional[str],
        proguard_mapping_file: List[str],
        comm_filter: Set[str],
        sample_filter: Optional[str]) -> GeckoProfile:
    """convert a simpleperf profile to gecko format"""
    lib = ReportLib()

    lib.ShowIpForUnknownSymbol()
    for file_path in proguard_mapping_file:
        lib.AddProguardMappingFile(file_path)
    if symfs_dir is not None:
        lib.SetSymfs(symfs_dir)
    lib.SetRecordFile(record_file)
    if kallsyms_file is not None:
        lib.SetKallsymsFile(kallsyms_file)
    if sample_filter:
        lib.SetSampleFilter(sample_filter)

    arch = lib.GetArch()
    meta_info = lib.MetaInfo()
    record_cmd = lib.GetRecordCmd()

    # Map from tid to Thread
    threadMap: Dict[int, Thread] = {}

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
        sample_time_ms = sample.time / 1000000

        stack = ['%s (in %s)' % (symbol.symbol_name, symbol.dso_name)]
        for i in range(callchain.nr):
            entry = callchain.entries[i]
            stack.append('%s (in %s)' % (entry.symbol.symbol_name, entry.symbol.dso_name))
        # We want root first, leaf last.
        stack.reverse()

        # add thread sample
        thread = threadMap.get(sample.tid)
        if thread is None:
            thread = Thread(comm=sample.thread_comm, pid=sample.pid, tid=sample.tid)
            threadMap[sample.tid] = thread
        thread._add_sample(
            comm=sample.thread_comm,
            stack=stack,
            # We are being a bit fast and loose here with time here.  simpleperf
            # uses CLOCK_MONOTONIC by default, which doesn't use the normal unix
            # epoch, but rather some arbitrary time. In practice, this doesn't
            # matter, the Firefox Profiler normalises all the timestamps to begin at
            # the minimum time.  Consider fixing this in future, if needed, by
            # setting `simpleperf record --clockid realtime`.
            time_ms=sample_time_ms)

    threads = [thread._to_json_dict() for thread in threadMap.values()]

    profile_timestamp = meta_info.get('timestamp')
    end_time_ms = (int(profile_timestamp) * 1000) if profile_timestamp else 0

    # Schema: https://github.com/firefox-devtools/profiler/blob/53970305b51b9b472e26d7457fee1d66cd4e2737/src/types/gecko-profile.js#L305
    gecko_profile_meta = {
        "interval": 1,
        "processType": 0,
        "product": record_cmd,
        "device": meta_info.get("product_props"),
        "platform": meta_info.get("android_build_fingerprint"),
        "stackwalk": 1,
        "debug": 0,
        "gcpoison": 0,
        "asyncstack": 1,
        # The profile timestamp is actually the end time, not the start time.
        # This is close enough for our purposes; I mostly just want to know which
        # day the profile was taken! Consider fixing this in future, if needed,
        # by setting `simpleperf record --clockid realtime` and taking the minimum
        # sample time.
        "startTime": end_time_ms,
        "shutdownTime": None,
        "version": 24,
        "presymbolicated": True,
        "categories": CATEGORIES,
        "markerSchema": [],
        "abi": arch,
        "oscpu": meta_info.get("android_build_fingerprint"),
    }

    # Schema:
    # https://github.com/firefox-devtools/profiler/blob/53970305b51b9b472e26d7457fee1d66cd4e2737/src/types/gecko-profile.js#L377
    # https://github.com/firefox-devtools/profiler/blob/main/docs-developer/gecko-profile-format.md
    return {
        "meta": gecko_profile_meta,
        "libs": [],
        "threads": threads,
        "processes": [],
        "pausedRanges": [],
    }


def main() -> None:
    parser = BaseArgumentParser(description=__doc__)
    parser.add_argument('--symfs',
                        help='Set the path to find binaries with symbols and debug info.')
    parser.add_argument('--kallsyms', help='Set the path to find kernel symbols.')
    parser.add_argument('-i', '--record_file', nargs='?', default='perf.data',
                        help='Default is perf.data.')
    parser.add_argument(
        '--proguard-mapping-file', nargs='+',
        help='Add proguard mapping file to de-obfuscate symbols',
        default=[])
    sample_filter_group = parser.add_argument_group('Sample filter options')
    parser.add_sample_filter_options(sample_filter_group)
    sample_filter_group.add_argument('--comm', nargs='+', action='append', help="""
      Use samples only in threads with selected names.""")
    args = parser.parse_args()
    profile = _gecko_profile(
        record_file=args.record_file,
        symfs_dir=args.symfs,
        kallsyms_file=args.kallsyms,
        proguard_mapping_file=args.proguard_mapping_file,
        comm_filter=set(flatten_arg_list(args.comm)),
        sample_filter=args.sample_filter)

    json.dump(profile, sys.stdout, sort_keys=True)


if __name__ == '__main__':
    main()
