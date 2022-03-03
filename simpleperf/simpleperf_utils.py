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

"""utils.py: export utility functions.
"""

from __future__ import annotations
import argparse
from concurrent.futures import Future, ThreadPoolExecutor
import logging
import os
import os.path
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from typing import Any, Dict, Iterator, List, Optional, Set, Tuple, Union


NDK_ERROR_MESSAGE = "Please install the Android NDK (https://developer.android.com/studio/projects/install-ndk), then set NDK path with --ndk_path option."


def get_script_dir() -> str:
    return os.path.dirname(os.path.realpath(__file__))


def is_windows() -> bool:
    return sys.platform == 'win32' or sys.platform == 'cygwin'


def is_darwin() -> bool:
    return sys.platform == 'darwin'


def get_platform() -> str:
    if is_windows():
        return 'windows'
    if is_darwin():
        return 'darwin'
    return 'linux'


def str_to_bytes(str_value: str) -> bytes:
    # In python 3, str are wide strings whereas the C api expects 8 bit strings,
    # hence we have to convert. For now using utf-8 as the encoding.
    return str_value.encode('utf-8')


def bytes_to_str(bytes_value: Optional[bytes]) -> str:
    if not bytes_value:
        return ''
    return bytes_value.decode('utf-8')


def get_target_binary_path(arch: str, binary_name: str) -> str:
    if arch == 'aarch64':
        arch = 'arm64'
    arch_dir = os.path.join(get_script_dir(), "bin", "android", arch)
    if not os.path.isdir(arch_dir):
        log_fatal("can't find arch directory: %s" % arch_dir)
    binary_path = os.path.join(arch_dir, binary_name)
    if not os.path.isfile(binary_path):
        log_fatal("can't find binary: %s" % binary_path)
    return binary_path


def get_host_binary_path(binary_name: str) -> str:
    dirname = os.path.join(get_script_dir(), 'bin')
    if is_windows():
        if binary_name.endswith('.so'):
            binary_name = binary_name[0:-3] + '.dll'
        elif '.' not in binary_name:
            binary_name += '.exe'
        dirname = os.path.join(dirname, 'windows')
    elif sys.platform == 'darwin':  # OSX
        if binary_name.endswith('.so'):
            binary_name = binary_name[0:-3] + '.dylib'
        dirname = os.path.join(dirname, 'darwin')
    else:
        dirname = os.path.join(dirname, 'linux')
    dirname = os.path.join(dirname, 'x86_64' if sys.maxsize > 2 ** 32 else 'x86')
    binary_path = os.path.join(dirname, binary_name)
    if not os.path.isfile(binary_path):
        log_fatal("can't find binary: %s" % binary_path)
    return binary_path


def is_executable_available(executable: str, option='--help') -> bool:
    """ Run an executable to see if it exists. """
    try:
        subproc = subprocess.Popen([executable, option], stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
        subproc.communicate()
        return subproc.returncode == 0
    except OSError:
        return False


class ToolFinder:
    """ Find tools in ndk or sdk. """
    DEFAULT_SDK_PATH = {
        'darwin': 'Library/Android/sdk',
        'linux': 'Android/Sdk',
        'windows': 'AppData/Local/Android/sdk',
    }

    EXPECTED_TOOLS = {
        'adb': {
            'is_binutils': False,
            'test_option': 'version',
            'path_in_sdk': 'platform-tools/adb',
        },
        'llvm-objdump': {
            'is_binutils': False,
            'path_in_ndk':
                lambda platform: 'toolchains/llvm/prebuilt/%s-x86_64/bin/llvm-objdump' % platform,
        },
        'llvm-readelf': {
            'is_binutils': False,
            'path_in_ndk':
                lambda platform: 'toolchains/llvm/prebuilt/%s-x86_64/bin/llvm-readelf' % platform,
        },
        'llvm-symbolizer': {
            'is_binutils': False,
            'path_in_ndk':
                lambda platform: 'toolchains/llvm/prebuilt/%s-x86_64/bin/llvm-symbolizer' % platform,
        },
        'llvm-strip': {
            'is_binutils': False,
            'path_in_ndk':
                lambda platform: 'toolchains/llvm/prebuilt/%s-x86_64/bin/llvm-strip' % platform,
        },
    }

    @classmethod
    def find_ndk_and_sdk_paths(cls, ndk_path: Optional[str] = None
                               ) -> Iterator[Tuple[Optional[str], Optional[str]]]:
        # Use the given ndk path.
        if ndk_path and os.path.isdir(ndk_path):
            ndk_path = os.path.abspath(ndk_path)
            yield ndk_path, cls.find_sdk_path(ndk_path)
        # Find ndk in the parent directory containing simpleperf scripts.
        ndk_path = os.path.dirname(os.path.abspath(get_script_dir()))
        yield ndk_path, cls.find_sdk_path(ndk_path)
        # Find ndk in the default sdk installation path.
        if is_windows():
            home = os.environ.get('HOMEDRIVE') + os.environ.get('HOMEPATH')
        else:
            home = os.environ.get('HOME')
        if home:
            platform = get_platform()
            sdk_path = os.path.join(home, cls.DEFAULT_SDK_PATH[platform].replace('/', os.sep))
            if os.path.isdir(sdk_path):
                path = os.path.join(sdk_path, 'ndk')
                if os.path.isdir(path):
                    # Android Studio can install multiple ndk versions in 'ndk'.
                    # Find the newest one.
                    ndk_version = None
                    for name in os.listdir(path):
                        if not ndk_version or ndk_version < name:
                            ndk_version = name
                    if ndk_version:
                        yield os.path.join(path, ndk_version), sdk_path
            ndk_path = os.path.join(sdk_path, 'ndk-bundle')
            if os.path.isdir(ndk_path):
                yield ndk_path, sdk_path

    @classmethod
    def find_sdk_path(cls, ndk_path: str) -> Optional[str]:
        path = ndk_path
        for _ in range(2):
            path = os.path.dirname(path)
            if os.path.isdir(os.path.join(path, 'platform-tools')):
                return path
        return None

    @classmethod
    def _get_binutils_path_in_ndk(cls, toolname: str, arch: Optional[str], platform: str
                                  ) -> Tuple[str, str]:
        if not arch:
            arch = 'arm64'
        if arch == 'arm64':
            name = 'aarch64-linux-android-' + toolname
        elif arch == 'arm':
            name = 'arm-linux-androideabi-' + toolname
        elif arch == 'x86_64':
            name = 'x86_64-linux-android-' + toolname
        elif arch == 'x86':
            name = 'i686-linux-android-' + toolname
        else:
            log_fatal('unexpected arch %s' % arch)
        path = 'toolchains/llvm/prebuilt/%s-x86_64/bin/%s' % (platform, name)
        return (name, path)

    @classmethod
    def find_tool_path(cls, toolname: str, ndk_path: Optional[str] = None,
                       arch: Optional[str] = None) -> Optional[str]:
        tool_info = cls.EXPECTED_TOOLS.get(toolname)
        if not tool_info:
            return None

        is_binutils = tool_info['is_binutils']
        test_option = tool_info.get('test_option', '--help')
        platform = get_platform()

        # Find tool in clang prebuilts in Android platform.
        if toolname.startswith('llvm-') and platform == 'linux' and get_script_dir().endswith(
                'system/extras/simpleperf/scripts'):
            path = str(
                Path(get_script_dir()).parents[3] / 'prebuilts' / 'clang' / 'host' / 'linux-x86' /
                'llvm-binutils-stable' / toolname)
            if is_executable_available(path, test_option):
                return path

        # Find tool in NDK or SDK.
        path_in_ndk = None
        path_in_sdk = None
        if is_binutils:
            toolname_with_arch, path_in_ndk = cls._get_binutils_path_in_ndk(
                toolname, arch, platform)
        else:
            toolname_with_arch = toolname
            if 'path_in_ndk' in tool_info:
                path_in_ndk = tool_info['path_in_ndk'](platform)
            elif 'path_in_sdk' in tool_info:
                path_in_sdk = tool_info['path_in_sdk']
        if path_in_ndk:
            path_in_ndk = path_in_ndk.replace('/', os.sep)
        elif path_in_sdk:
            path_in_sdk = path_in_sdk.replace('/', os.sep)

        for ndk_dir, sdk_dir in cls.find_ndk_and_sdk_paths(ndk_path):
            if path_in_ndk and ndk_dir:
                path = os.path.join(ndk_dir, path_in_ndk)
                if is_executable_available(path, test_option):
                    return path
            elif path_in_sdk and sdk_dir:
                path = os.path.join(sdk_dir, path_in_sdk)
                if is_executable_available(path, test_option):
                    return path

        # Find tool in $PATH.
        if is_executable_available(toolname_with_arch, test_option):
            return toolname_with_arch

        # Find tool without arch in $PATH.
        if is_binutils and tool_info.get('accept_tool_without_arch'):
            if is_executable_available(toolname, test_option):
                return toolname
        return None


class AdbHelper(object):
    def __init__(self, enable_switch_to_root: bool = True):
        adb_path = ToolFinder.find_tool_path('adb')
        if not adb_path:
            log_exit("Can't find adb in PATH environment.")
        self.adb_path: str = adb_path
        self.enable_switch_to_root = enable_switch_to_root
        self.serial_number: Optional[str] = None

    def is_device_available(self) -> bool:
        return self.run_and_return_output(['shell', 'whoami'])[0]

    def run(self, adb_args: List[str], log_output: bool = False, log_stderr: bool = False) -> bool:
        return self.run_and_return_output(adb_args, log_output, log_stderr)[0]

    def run_and_return_output(self, adb_args: List[str], log_output: bool = False,
                              log_stderr: bool = False) -> Tuple[bool, str]:
        adb_args = [self.adb_path] + adb_args
        logging.debug('run adb cmd: %s' % adb_args)
        env = None
        if self.serial_number:
            env = os.environ.copy()
            env['ANDROID_SERIAL'] = self.serial_number
        subproc = subprocess.Popen(
            adb_args, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout_data, stderr_data = subproc.communicate()
        stdout_data = bytes_to_str(stdout_data)
        stderr_data = bytes_to_str(stderr_data)
        returncode = subproc.returncode
        result = (returncode == 0)
        if log_output and stdout_data:
            logging.debug(stdout_data)
        if log_stderr and stderr_data:
            logging.warning(stderr_data)
        logging.debug('run adb cmd: %s  [result %s]' % (adb_args, result))
        return (result, stdout_data)

    def check_run(self, adb_args: List[str], log_output: bool = False):
        self.check_run_and_return_output(adb_args, log_output)

    def check_run_and_return_output(self, adb_args: List[str], log_output: bool = False,
                                    log_stderr: bool = False) -> str:
        result, stdoutdata = self.run_and_return_output(adb_args, log_output, True)
        if not result:
            log_exit('run "adb %s" failed: %s' % (adb_args, stdoutdata))
        return stdoutdata

    def _unroot(self):
        result, stdoutdata = self.run_and_return_output(['shell', 'whoami'])
        if not result:
            return
        if 'root' not in stdoutdata:
            return
        logging.info('unroot adb')
        self.run(['unroot'])
        time.sleep(1)
        self.run(['wait-for-device'])

    def switch_to_root(self) -> bool:
        if not self.enable_switch_to_root:
            self._unroot()
            return False
        result, stdoutdata = self.run_and_return_output(['shell', 'whoami'])
        if not result:
            return False
        if 'root' in stdoutdata:
            return True
        build_type = self.get_property('ro.build.type')
        if build_type == 'user':
            return False
        self.run(['root'])
        time.sleep(1)
        self.run(['wait-for-device'])
        result, stdoutdata = self.run_and_return_output(['shell', 'whoami'])
        return result and 'root' in stdoutdata

    def get_property(self, name: str) -> Optional[str]:
        result, stdoutdata = self.run_and_return_output(['shell', 'getprop', name])
        return stdoutdata.strip() if result else None

    def set_property(self, name: str, value: str) -> bool:
        return self.run(['shell', 'setprop', name, value])

    def get_device_arch(self) -> str:
        output = self.check_run_and_return_output(['shell', 'uname', '-m'])
        if 'aarch64' in output:
            return 'arm64'
        if 'arm' in output:
            return 'arm'
        if 'x86_64' in output:
            return 'x86_64'
        if '86' in output:
            return 'x86'
        log_fatal('unsupported architecture: %s' % output.strip())
        return ''

    def get_android_version(self) -> int:
        """ Get Android version on device, like 7 is for Android N, 8 is for Android O."""
        build_version = self.get_property('ro.build.version.codename')
        if not build_version or build_version == 'REL':
            build_version = self.get_property('ro.build.version.release')
        android_version = 0
        if build_version:
            if build_version[0].isdigit():
                i = 1
                while i < len(build_version) and build_version[i].isdigit():
                    i += 1
                android_version = int(build_version[:i])
            else:
                c = build_version[0].upper()
                if c.isupper() and c >= 'L':
                    android_version = ord(c) - ord('L') + 5
        return android_version


def flatten_arg_list(arg_list: List[List[str]]) -> List[str]:
    res = []
    if arg_list:
        for items in arg_list:
            res += items
    return res


def remove(dir_or_file: Union[Path, str]):
    if os.path.isfile(dir_or_file):
        os.remove(dir_or_file)
    elif os.path.isdir(dir_or_file):
        shutil.rmtree(dir_or_file, ignore_errors=True)


def open_report_in_browser(report_path: str):
    if is_darwin():
        # On darwin 10.12.6, webbrowser can't open browser, so try `open` cmd first.
        try:
            subprocess.check_call(['open', report_path])
            return
        except subprocess.CalledProcessError:
            pass
    import webbrowser
    try:
        # Try to open the report with Chrome
        browser = webbrowser.get('google-chrome')
        browser.open(report_path, new=0, autoraise=True)
    except webbrowser.Error:
        # webbrowser.get() doesn't work well on darwin/windows.
        webbrowser.open_new_tab(report_path)


class BinaryFinder:
    def __init__(self, binary_cache_dir: Optional[Union[Path, str]], readelf: ReadElf):
        if isinstance(binary_cache_dir, str):
            binary_cache_dir = Path(binary_cache_dir)
        self.binary_cache_dir = binary_cache_dir
        self.readelf = readelf
        self.build_id_map = self._load_build_id_map()

    def _load_build_id_map(self) -> Dict[str, Path]:
        build_id_map: Dict[str, Path] = {}
        if self.binary_cache_dir:
            build_id_list_file = self.binary_cache_dir / 'build_id_list'
            if build_id_list_file.is_file():
                with open(self.binary_cache_dir / 'build_id_list', 'rb') as fh:
                    for line in fh.readlines():
                        # lines are in format "<build_id>=<path_in_binary_cache>".
                        items = bytes_to_str(line).strip().split('=')
                        if len(items) == 2:
                            build_id_map[items[0]] = self.binary_cache_dir / items[1]
        return build_id_map

    def find_binary(self, dso_path_in_record_file: str,
                    expected_build_id: Optional[str]) -> Optional[Path]:
        """ If expected_build_id is None, don't check build id.
            Otherwise, the build id of the found binary should match the expected one."""
        # Find binary from build id map.
        if expected_build_id:
            path = self.build_id_map.get(expected_build_id)
            if path and self._check_path(path, expected_build_id):
                return path
        # Find binary by path in binary cache.
        if self.binary_cache_dir:
            path = self.binary_cache_dir / dso_path_in_record_file[1:]
            if self._check_path(path, expected_build_id):
                return path
        # Find binary by its absolute path.
        path = Path(dso_path_in_record_file)
        if self._check_path(path, expected_build_id):
            return path
        return None

    def _check_path(self, path: Path, expected_build_id: Optional[str]) -> bool:
        if not self.readelf.is_elf_file(path):
            return False
        if expected_build_id is not None:
            return self.readelf.get_build_id(path) == expected_build_id
        return True


class Addr2Nearestline(object):
    """ Use llvm-symbolizer to convert (dso_path, func_addr, addr) to (source_file, line).
        For instructions generated by C++ compilers without a matching statement in source code
        (like stack corruption check, switch optimization, etc.), addr2line can't generate
        line information. However, we want to assign the instruction to the nearest line before
        the instruction (just like objdump -dl). So we use below strategy:
        Instead of finding the exact line of the instruction in an address, we find the nearest
        line to the instruction in an address. If an address doesn't have a line info, we find
        the line info of address - 1. If still no line info, then use address - 2, address - 3,
        etc.

        The implementation steps are as below:
        1. Collect all (dso_path, func_addr, addr) requests before converting. This saves the
        times to call addr2line.
        2. Convert addrs to (source_file, line) pairs for each dso_path as below:
          2.1 Check if the dso_path has .debug_line. If not, omit its conversion.
          2.2 Get arch of the dso_path, and decide the addr_step for it. addr_step is the step we
          change addr each time. For example, since instructions of arm64 are all 4 bytes long,
          addr_step for arm64 can be 4.
          2.3 Use addr2line to find line info for each addr in the dso_path.
          2.4 For each addr without line info, use addr2line to find line info for
              range(addr - addr_step, addr - addr_step * 4 - 1, -addr_step).
          2.5 For each addr without line info, use addr2line to find line info for
              range(addr - addr_step * 5, addr - addr_step * 128 - 1, -addr_step).
              (128 is a guess number. A nested switch statement in
               system/core/demangle/Demangler.cpp has >300 bytes without line info in arm64.)
    """
    class Dso(object):
        """ Info of a dynamic shared library.
            addrs: a map from address to Addr object in this dso.
        """

        def __init__(self, build_id: Optional[str]):
            self.build_id = build_id
            self.addrs: Dict[int, Addr2Nearestline.Addr] = {}
            # Saving file names for each addr takes a lot of memory. So we store file ids in Addr,
            # and provide data structures connecting file id and file name here.
            self.file_name_to_id: Dict[str, int] = {}
            self.file_id_to_name: List[str] = []
            self.func_name_to_id: Dict[str, int] = {}
            self.func_id_to_name: List[str] = []

        def get_file_id(self, file_path: str) -> int:
            file_id = self.file_name_to_id.get(file_path)
            if file_id is None:
                file_id = self.file_name_to_id[file_path] = len(self.file_id_to_name)
                self.file_id_to_name.append(file_path)
            return file_id

        def get_func_id(self, func_name: str) -> int:
            func_id = self.func_name_to_id.get(func_name)
            if func_id is None:
                func_id = self.func_name_to_id[func_name] = len(self.func_id_to_name)
                self.func_id_to_name.append(func_name)
            return func_id

    class Addr(object):
        """ Info of an addr request.
            func_addr: start_addr of the function containing addr.
            source_lines: a list of [file_id, line_number] for addr.
                          source_lines[:-1] are all for inlined functions.
        """

        def __init__(self, func_addr: int):
            self.func_addr = func_addr
            self.source_lines: Optional[List[int, int]] = None

    def __init__(
            self, ndk_path: Optional[str],
            binary_finder: BinaryFinder, with_function_name: bool):
        self.symbolizer_path = ToolFinder.find_tool_path('llvm-symbolizer', ndk_path)
        if not self.symbolizer_path:
            log_exit("Can't find llvm-symbolizer. " + NDK_ERROR_MESSAGE)
        self.readelf = ReadElf(ndk_path)
        self.dso_map: Dict[str, Addr2Nearestline.Dso] = {}  # map from dso_path to Dso.
        self.binary_finder = binary_finder
        self.with_function_name = with_function_name

    def add_addr(self, dso_path: str, build_id: Optional[str], func_addr: int, addr: int):
        dso = self.dso_map.get(dso_path)
        if dso is None:
            dso = self.dso_map[dso_path] = self.Dso(build_id)
        if addr not in dso.addrs:
            dso.addrs[addr] = self.Addr(func_addr)

    def convert_addrs_to_lines(self, jobs: int):
        with ThreadPoolExecutor(jobs) as executor:
            futures: List[Future] = []
            for dso_path, dso in self.dso_map.items():
                futures.append(executor.submit(self._convert_addrs_in_one_dso, dso_path, dso))
            for future in futures:
                # Call future.result() to report exceptions raised in the executor.
                future.result()

    def _convert_addrs_in_one_dso(self, dso_path: str, dso: Addr2Nearestline.Dso):
        real_path = self.binary_finder.find_binary(dso_path, dso.build_id)
        if not real_path:
            if dso_path not in ['//anon', 'unknown', '[kernel.kallsyms]']:
                logging.debug("Can't find dso %s" % dso_path)
            return

        if not self._check_debug_line_section(real_path):
            logging.debug("file %s doesn't contain .debug_line section." % real_path)
            return

        addr_step = self._get_addr_step(real_path)
        self._collect_line_info(dso, real_path, [0])
        self._collect_line_info(dso, real_path, range(-addr_step, -addr_step * 4 - 1, -addr_step))
        self._collect_line_info(dso, real_path,
                                range(-addr_step * 5, -addr_step * 128 - 1, -addr_step))

    def _check_debug_line_section(self, real_path: Path) -> bool:
        return '.debug_line' in self.readelf.get_sections(real_path)

    def _get_addr_step(self, real_path: Path) -> int:
        arch = self.readelf.get_arch(real_path)
        if arch == 'arm64':
            return 4
        if arch == 'arm':
            return 2
        return 1

    def _collect_line_info(
            self, dso: Addr2Nearestline.Dso, real_path: Path, addr_shifts: List[int]):
        """ Use addr2line to get line info in a dso, with given addr shifts. """
        # 1. Collect addrs to send to addr2line.
        addr_set: Set[int] = set()
        for addr in dso.addrs:
            addr_obj = dso.addrs[addr]
            if addr_obj.source_lines:  # already has source line, no need to search.
                continue
            for shift in addr_shifts:
                # The addr after shift shouldn't change to another function.
                shifted_addr = max(addr + shift, addr_obj.func_addr)
                addr_set.add(shifted_addr)
                if shifted_addr == addr_obj.func_addr:
                    break
        if not addr_set:
            return
        addr_request = '\n'.join(['0x%x' % addr for addr in sorted(addr_set)])

        # 2. Use addr2line to collect line info.
        try:
            subproc = subprocess.Popen(self._build_symbolizer_args(real_path),
                                       stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            (stdoutdata, _) = subproc.communicate(str_to_bytes(addr_request))
            stdoutdata = bytes_to_str(stdoutdata)
        except OSError:
            return
        addr_map = self.parse_line_output(stdoutdata, dso)

        # 3. Fill line info in dso.addrs.
        for addr in dso.addrs:
            addr_obj = dso.addrs[addr]
            if addr_obj.source_lines:
                continue
            for shift in addr_shifts:
                shifted_addr = max(addr + shift, addr_obj.func_addr)
                lines = addr_map.get(shifted_addr)
                if lines:
                    addr_obj.source_lines = lines
                    break
                if shifted_addr == addr_obj.func_addr:
                    break

    def _build_symbolizer_args(self, binary_path: Path) -> List[str]:
        args = [self.symbolizer_path, '--print-address', '--inlining', '--obj=%s' % binary_path]
        if self.with_function_name:
            args += ['--functions=linkage', '--demangle']
        else:
            args.append('--functions=none')
        return args

    def parse_line_output(self, output: str, dso: Addr2Nearestline.Dso) -> Dict[int,
                                                                                List[Tuple[int]]]:
        """
        The output is a list of lines.
            address1
            function_name1 (the function name can be empty)
            source_location1
            function_name2
            source_location2
            ...
            (end with empty line)
        """

        addr_map: Dict[int, List[Tuple[int]]] = {}
        lines = output.strip().splitlines()
        i = 0
        while i < len(lines):
            address = self._parse_line_output_address(lines[i])
            i += 1
            if address is None:
                continue
            info = []
            while i < len(lines):
                if self.with_function_name:
                    if i + 1 == len(lines):
                        break
                    function_name = lines[i].strip()
                    if not function_name and (':' not in lines[i+1]):
                        # no more frames
                        break
                    i += 1
                elif not lines[i]:
                    i += 1
                    break

                file_path, line_number = self._parse_line_output_source_location(lines[i])
                i += 1
                if not file_path or not line_number:
                    # An addr can have a list of (file, line), when the addr belongs to an inlined
                    # function. Sometimes only part of the list has ? mark. In this case, we think
                    # the line info is valid if the first line doesn't have ? mark.
                    if not info:
                        break
                    continue
                file_id = dso.get_file_id(file_path)
                if self.with_function_name:
                    func_id = dso.get_func_id(function_name)
                    info.append((file_id, line_number, func_id))
                else:
                    info.append((file_id, line_number))
            if info:
                addr_map[address] = info
        return addr_map

    def _parse_line_output_address(self, output: str) -> Optional[int]:
        if output.startswith('0x'):
            return int(output, 16)
        return None

    def _parse_line_output_source_location(self, line: str) -> Tuple[Optional[str], Optional[int]]:
        file_path, line_number = None, None
        # Handle lines in format filename:line:column, like "runtest/two_functions.cpp:14:25".
        # Filename may contain ':' like "C:\Users\...\file".
        items = line.rsplit(':', 2)
        if len(items) == 3:
            file_path, line_number = items[:2]
        if not file_path or ('?' in file_path) or not line_number or ('?' in line_number):
            return None, None
        try:
            line_number = int(line_number)
        except ValueError:
            return None, None
        return file_path, line_number

    def get_dso(self, dso_path: str) -> Addr2Nearestline.Dso:
        return self.dso_map.get(dso_path)

    def get_addr_source(self, dso: Addr2Nearestline.Dso, addr: int) -> Optional[List[Tuple[int]]]:
        source = dso.addrs[addr].source_lines
        if source is None:
            return None
        if self.with_function_name:
            return [(dso.file_id_to_name[file_id], line, dso.func_id_to_name[func_id])
                    for (file_id, line, func_id) in source]
        return [(dso.file_id_to_name[file_id], line) for (file_id, line) in source]


class SourceFileSearcher(object):
    """ Find source file paths in the file system.
        The file paths reported by addr2line are the paths stored in debug sections
        of shared libraries. And we need to convert them to file paths in the file
        system. It is done in below steps:
        1. Collect all file paths under the provided source_dirs. The suffix of a
           source file should contain one of below:
            h: for C/C++ header files.
            c: for C/C++ source files.
            java: for Java source files.
            kt: for Kotlin source files.
        2. Given an abstract_path reported by addr2line, select the best real path
           as below:
           2.1 Find all real paths with the same file name as the abstract path.
           2.2 Select the real path having the longest common suffix with the abstract path.
    """

    SOURCE_FILE_EXTS = {'.h', '.hh', '.H', '.hxx', '.hpp', '.h++',
                        '.c', '.cc', '.C', '.cxx', '.cpp', '.c++',
                        '.java', '.kt'}

    @classmethod
    def is_source_filename(cls, filename: str) -> bool:
        ext = os.path.splitext(filename)[1]
        return ext in cls.SOURCE_FILE_EXTS

    def __init__(self, source_dirs: List[str]):
        # Map from filename to a list of reversed directory path containing filename.
        self.filename_to_rparents: Dict[str, List[str]] = {}
        self._collect_paths(source_dirs)

    def _collect_paths(self, source_dirs: List[str]):
        for source_dir in source_dirs:
            for parent, _, file_names in os.walk(source_dir):
                rparent = None
                for file_name in file_names:
                    if self.is_source_filename(file_name):
                        rparents = self.filename_to_rparents.get(file_name)
                        if rparents is None:
                            rparents = self.filename_to_rparents[file_name] = []
                        if rparent is None:
                            rparent = parent[::-1]
                        rparents.append(rparent)

    def get_real_path(self, abstract_path: str) -> Optional[str]:
        abstract_path = abstract_path.replace('/', os.sep)
        abstract_parent, file_name = os.path.split(abstract_path)
        abstract_rparent = abstract_parent[::-1]
        real_rparents = self.filename_to_rparents.get(file_name)
        if real_rparents is None:
            return None
        best_matched_rparent = None
        best_common_length = -1
        for real_rparent in real_rparents:
            length = len(os.path.commonprefix((real_rparent, abstract_rparent)))
            if length > best_common_length:
                best_common_length = length
                best_matched_rparent = real_rparent
        if best_matched_rparent is None:
            return None
        return os.path.join(best_matched_rparent[::-1], file_name)


class Objdump(object):
    """ A wrapper of objdump to disassemble code. """

    def __init__(self, ndk_path: Optional[str], binary_finder: BinaryFinder):
        self.ndk_path = ndk_path
        self.binary_finder = binary_finder
        self.readelf = ReadElf(ndk_path)
        self.objdump_paths: Dict[str, str] = {}

    def get_dso_info(self, dso_path: str, expected_build_id: Optional[str]
                     ) -> Optional[Tuple[str, str]]:
        real_path = self.binary_finder.find_binary(dso_path, expected_build_id)
        if not real_path:
            return None
        arch = self.readelf.get_arch(real_path)
        if arch == 'unknown':
            return None
        return (str(real_path), arch)

    def disassemble_code(self, dso_info, start_addr, addr_len) -> List[Tuple[str, int]]:
        """ Disassemble [start_addr, start_addr + addr_len] of dso_path.
            Return a list of pair (disassemble_code_line, addr).
        """
        real_path, arch = dso_info
        objdump_path = self.objdump_paths.get(arch)
        if not objdump_path:
            objdump_path = ToolFinder.find_tool_path('llvm-objdump', self.ndk_path, arch)
            if not objdump_path:
                log_exit("Can't find llvm-objdump." + NDK_ERROR_MESSAGE)
            self.objdump_paths[arch] = objdump_path

        # 3. Run objdump.
        args = [objdump_path, '-dlC', '--no-show-raw-insn',
                '--start-address=0x%x' % start_addr,
                '--stop-address=0x%x' % (start_addr + addr_len),
                real_path]
        if arch == 'arm' and 'llvm-objdump' in objdump_path:
            args += ['--print-imm-hex']
        try:
            subproc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            (stdoutdata, _) = subproc.communicate()
            stdoutdata = bytes_to_str(stdoutdata)
        except OSError:
            return None

        if not stdoutdata:
            return None
        result = []
        for line in stdoutdata.split('\n'):
            line = line.rstrip()  # Remove '\r' on Windows.
            items = line.split(':', 1)
            try:
                addr = int(items[0], 16)
            except ValueError:
                addr = 0
            result.append((line, addr))
        return result


class ReadElf(object):
    """ A wrapper of readelf. """

    def __init__(self, ndk_path: Optional[str]):
        self.readelf_path = ToolFinder.find_tool_path('llvm-readelf', ndk_path)
        if not self.readelf_path:
            log_exit("Can't find llvm-readelf. " + NDK_ERROR_MESSAGE)

    @staticmethod
    def is_elf_file(path: Union[Path, str]) -> bool:
        if os.path.isfile(path):
            with open(path, 'rb') as fh:
                return fh.read(4) == b'\x7fELF'
        return False

    def get_arch(self, elf_file_path: Union[Path, str]) -> str:
        """ Get arch of an elf file. """
        if self.is_elf_file(elf_file_path):
            try:
                output = subprocess.check_output([self.readelf_path, '-h', str(elf_file_path)])
                output = bytes_to_str(output)
                if output.find('AArch64') != -1:
                    return 'arm64'
                if output.find('ARM') != -1:
                    return 'arm'
                if output.find('X86-64') != -1:
                    return 'x86_64'
                if output.find('80386') != -1:
                    return 'x86'
            except subprocess.CalledProcessError:
                pass
        return 'unknown'

    def get_build_id(self, elf_file_path: Union[Path, str], with_padding=True) -> str:
        """ Get build id of an elf file. """
        if self.is_elf_file(elf_file_path):
            try:
                output = subprocess.check_output([self.readelf_path, '-n', str(elf_file_path)])
                output = bytes_to_str(output)
                result = re.search(r'Build ID:\s*(\S+)', output)
                if result:
                    build_id = result.group(1)
                    if with_padding:
                        build_id = self.pad_build_id(build_id)
                    return build_id
            except subprocess.CalledProcessError:
                pass
        return ""

    @staticmethod
    def pad_build_id(build_id: str) -> str:
        """ Pad build id to 40 hex numbers (20 bytes). """
        if len(build_id) < 40:
            build_id += '0' * (40 - len(build_id))
        else:
            build_id = build_id[:40]
        return '0x' + build_id

    @staticmethod
    def unpad_build_id(build_id: str) -> str:
        if build_id.startswith('0x'):
            build_id = build_id[2:]
            # Unpad build id as TrimZeroesFromBuildIDString() in quipper.
            padding = '0' * 8
            while build_id.endswith(padding):
                build_id = build_id[:-len(padding)]
        return build_id

    def get_sections(self, elf_file_path: Union[Path, str]) -> List[str]:
        """ Get sections of an elf file. """
        section_names: List[str] = []
        if self.is_elf_file(elf_file_path):
            try:
                output = subprocess.check_output([self.readelf_path, '-SW', str(elf_file_path)])
                output = bytes_to_str(output)
                for line in output.split('\n'):
                    # Parse line like:" [ 1] .note.android.ident NOTE  0000000000400190 ...".
                    result = re.search(r'^\s+\[\s*\d+\]\s(.+?)\s', line)
                    if result:
                        section_name = result.group(1).strip()
                        if section_name:
                            section_names.append(section_name)
            except subprocess.CalledProcessError:
                pass
        return section_names


def extant_dir(arg: str) -> str:
    """ArgumentParser type that only accepts extant directories.

    Args:
        arg: The string argument given on the command line.
    Returns: The argument as a realpath.
    Raises:
        argparse.ArgumentTypeError: The given path isn't a directory.
    """
    path = os.path.realpath(arg)
    if not os.path.isdir(path):
        raise argparse.ArgumentTypeError('{} is not a directory.'.format(path))
    return path


def extant_file(arg: str) -> str:
    """ArgumentParser type that only accepts extant files.

    Args:
        arg: The string argument given on the command line.
    Returns: The argument as a realpath.
    Raises:
        argparse.ArgumentTypeError: The given path isn't a file.
    """
    path = os.path.realpath(arg)
    if not os.path.isfile(path):
        raise argparse.ArgumentTypeError('{} is not a file.'.format(path))
    return path


def log_fatal(msg: str):
    raise Exception(msg)


def log_exit(msg: str):
    sys.exit(msg)


class LogFormatter(logging.Formatter):
    """ Use custom logging format. """

    def __init__(self):
        super().__init__('%(asctime)s [%(levelname)s] (%(filename)s:%(lineno)d) %(message)s')

    def formatTime(self, record, datefmt):
        return super().formatTime(record, '%H:%M:%S') + ',%03d' % record.msecs


class Log:
    initialized = False

    @classmethod
    def init(cls, log_level: str = 'info'):
        assert not cls.initialized
        cls.initialized = True
        cls.logger = logging.root
        cls.logger.setLevel(log_level.upper())
        handler = logging.StreamHandler()
        handler.setFormatter(LogFormatter())
        cls.logger.addHandler(handler)


class ArgParseFormatter(
        argparse.ArgumentDefaultsHelpFormatter, argparse.RawDescriptionHelpFormatter):
    pass


class BaseArgumentParser(argparse.ArgumentParser):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs, formatter_class=ArgParseFormatter)
        self.has_sample_filter_options = False
        self.sample_filter_with_pid_shortcut = False

    def add_trace_offcpu_option(self, subparser: Optional[Any] = None):
        parser = subparser if subparser else self
        parser.add_argument(
            '--trace-offcpu', choices=['on-cpu', 'off-cpu', 'on-off-cpu', 'mixed-on-off-cpu'],
            help="""Set report mode for profiles recorded with --trace-offcpu option. All possible
                    modes are: on-cpu (only on-cpu samples), off-cpu (only off-cpu samples),
                    on-off-cpu (both on-cpu and off-cpu samples, can be split by event name),
                    mixed-on-off-cpu (on-cpu and off-cpu samples using the same event name).
                    If not set, mixed-on-off-cpu mode is used.
                """)

    def add_sample_filter_options(
            self, group: Optional[Any] = None, with_pid_shortcut: bool = True):
        if not group:
            group = self.add_argument_group('Sample filter options')
        group.add_argument('--exclude-pid', metavar='pid', nargs='+', type=int,
                           help='exclude samples for selected processes')
        group.add_argument('--exclude-tid', metavar='tid', nargs='+', type=int,
                           help='exclude samples for selected threads')
        group.add_argument(
            '--exclude-process-name', metavar='process_name_regex', nargs='+',
            help='exclude samples for processes with name containing the regular expression')
        group.add_argument(
            '--exclude-thread-name', metavar='thread_name_regex', nargs='+',
            help='exclude samples for threads with name containing the regular expression')

        if with_pid_shortcut:
            group.add_argument('--pid', metavar='pid', nargs='+', type=int,
                               help='only include samples for selected processes')
            group.add_argument('--tid', metavar='tid', nargs='+', type=int,
                               help='only include samples for selected threads')
        group.add_argument('--include-pid', metavar='pid', nargs='+', type=int,
                           help='only include samples for selected processes')
        group.add_argument('--include-tid', metavar='tid', nargs='+', type=int,
                           help='only include samples for selected threads')
        group.add_argument(
            '--include-process-name', metavar='process_name_regex', nargs='+',
            help='only include samples for processes with name containing the regular expression')
        group.add_argument(
            '--include-thread-name', metavar='thread_name_regex', nargs='+',
            help='only include samples for threads with name containing the regular expression')
        group.add_argument(
            '--filter-file', metavar='file',
            help='use filter file to filter samples based on timestamps. ' +
            'The file format is in doc/sampler_filter.md.')
        self.has_sample_filter_options = True
        self.sample_filter_with_pid_shortcut = with_pid_shortcut

    def _build_sample_filter(self, args: argparse.Namespace) -> Optional[str]:
        """ Convert sample filter options into a sample filter string, which can be passed to
            ReportLib.SetSampleFilter().
        """
        filters = []
        if args.exclude_pid:
            filters.append('--exclude-pid ' + ','.join(str(pid) for pid in args.exclude_pid))
        if args.exclude_tid:
            filters.append('--exclude-tid ' + ','.join(str(tid) for tid in args.exclude_tid))
        if args.exclude_process_name:
            for name in args.exclude_process_name:
                filters.append('--exclude-process-name ' + name)
        if args.exclude_thread_name:
            for name in args.exclude_thread_name:
                filters.append('--exclude-thread-name ' + name)

        if args.include_pid:
            filters.append('--include-pid ' + ','.join(str(pid) for pid in args.include_pid))
        if args.include_tid:
            filters.append('--include-tid ' + ','.join(str(tid) for tid in args.include_tid))
        if self.sample_filter_with_pid_shortcut:
            if args.pid:
                filters.append('--include-pid ' + ','.join(str(pid) for pid in args.pid))
            if args.tid:
                filters.append('--include-tid ' + ','.join(str(pid) for pid in args.tid))
        if args.include_process_name:
            for name in args.include_process_name:
                filters.append('--include-process-name ' + name)
        if args.include_thread_name:
            for name in args.include_thread_name:
                filters.append('--include-thread-name ' + name)
        if args.filter_file:
            filters.append('--filter-file ' + args.filter_file)
        return ' '.join(filters)

    def parse_known_args(self, *args, **kwargs):
        self.add_argument(
            '--log', choices=['debug', 'info', 'warning'],
            default='info', help='set log level')
        namespace, left_args = super().parse_known_args(*args, **kwargs)

        if self.has_sample_filter_options:
            setattr(namespace, 'sample_filter', self._build_sample_filter(namespace))

        if not Log.initialized:
            Log.init(namespace.log)
        return namespace, left_args
