#!/usr/bin/env python3
#
# Copyright (C) 2019 The Android Open Source Project
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

"""
    This script is part of controlling simpleperf recording in user code. It is used to prepare
    profiling environment (upload simpleperf to device and enable profiling) before recording
    and collect recording data on host after recording.
    Controlling simpleperf recording is done in below steps:
    1. Add simpleperf Java API/C++ API to the app's source code. And call the API in user code.
    2. Run `api_profiler.py prepare` to prepare profiling environment.
    3. Run the app one or more times to generate recording data.
    4. Run `api_profiler.py collect` to collect recording data on host.
"""

from argparse import Namespace
import logging
import os
import os.path
import shutil
import zipfile

from simpleperf_utils import (AdbHelper, BaseArgumentParser,
                              get_target_binary_path, log_exit, remove)


class ApiProfiler:
    def __init__(self, args: Namespace):
        self.args = args
        self.adb = AdbHelper()

    def prepare_recording(self):
        self.enable_profiling_on_device()
        self.upload_simpleperf_to_device()
        self.run_simpleperf_prepare_cmd()

    def enable_profiling_on_device(self):
        android_version = self.adb.get_android_version()
        if android_version >= 10:
            self.adb.set_property('debug.perf_event_max_sample_rate',
                                  str(self.args.max_sample_rate))
            self.adb.set_property('debug.perf_cpu_time_max_percent', str(self.args.max_cpu_percent))
            self.adb.set_property('debug.perf_event_mlock_kb', str(self.args.max_memory_in_kb))
        self.adb.set_property('security.perf_harden', '0')

    def upload_simpleperf_to_device(self):
        device_arch = self.adb.get_device_arch()
        simpleperf_binary = get_target_binary_path(device_arch, 'simpleperf')
        self.adb.check_run(['push', simpleperf_binary, '/data/local/tmp'])
        self.adb.check_run(['shell', 'chmod', 'a+x', '/data/local/tmp/simpleperf'])

    def run_simpleperf_prepare_cmd(self):
        cmd_args = ['shell', '/data/local/tmp/simpleperf', 'api-prepare', '--app', self.args.app]
        if self.args.days:
            cmd_args += ['--days', str(self.args.days)]
        self.adb.check_run(cmd_args)

    def collect_data(self):
        if not os.path.isdir(self.args.out_dir):
            os.makedirs(self.args.out_dir)
        self.download_recording_data()
        self.unzip_recording_data()

    def download_recording_data(self):
        """ download recording data to simpleperf_data.zip."""
        self.upload_simpleperf_to_device()
        self.adb.check_run(['shell', '/data/local/tmp/simpleperf', 'api-collect',
                            '--app', self.args.app, '-o', '/data/local/tmp/simpleperf_data.zip'])
        self.adb.check_run(['pull', '/data/local/tmp/simpleperf_data.zip', self.args.out_dir])
        self.adb.check_run(['shell', 'rm', '-rf', '/data/local/tmp/simpleperf_data'])

    def unzip_recording_data(self):
        zip_file_path = os.path.join(self.args.out_dir, 'simpleperf_data.zip')
        with zipfile.ZipFile(zip_file_path, 'r') as zip_fh:
            names = zip_fh.namelist()
            logging.info('There are %d recording data files.' % len(names))
            for name in names:
                logging.info('recording file: %s' % os.path.join(self.args.out_dir, name))
                zip_fh.extract(name, self.args.out_dir)
        remove(zip_file_path)


def main():
    parser = BaseArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(title='actions', dest='command')

    prepare_parser = subparsers.add_parser('prepare', help='Prepare recording on device.')
    prepare_parser.add_argument('-p', '--app', required=True, help="""
                                The app package name of the app profiled.""")
    prepare_parser.add_argument('-d', '--days', type=int, help="""
                                By default, the recording permission is reset after device reboot.
                                But on Android >= 13, we can use --days to set how long we want the
                                permission to persist. It can last after device reboot.
                                """)
    prepare_parser.add_argument('--max-sample-rate', type=int, default=100000, help="""
                                Set max sample rate (only on Android >= Q).""")
    prepare_parser.add_argument('--max-cpu-percent', type=int, default=25, help="""
                                Set max cpu percent for recording (only on Android >= Q).""")
    prepare_parser.add_argument('--max-memory-in-kb', type=int,
                                default=(1024 + 1) * 4 * 8, help="""
                                Set max kernel buffer size for recording (only on Android >= Q).
                                """)

    collect_parser = subparsers.add_parser('collect', help='Collect recording data.')
    collect_parser.add_argument('-p', '--app', required=True, help="""
                                The app package name of the app profiled.""")
    collect_parser.add_argument('-o', '--out-dir', default='simpleperf_data', help="""
                                The directory to store recording data.""")
    args = parser.parse_args()

    if args.command == 'prepare':
        ApiProfiler(args).prepare_recording()
    elif args.command == 'collect':
        ApiProfiler(args).collect_data()


if __name__ == '__main__':
    main()
