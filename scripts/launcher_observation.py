"""Bounded module observations and native CMD invocation, shared by probes.

Required dependency assertions accept STABLE only. EXITED/UNKNOWN/ERROR are
evidence failures, never an empty module list interpreted as isolation proof.
"""
import ctypes as c
from ctypes import wintypes as w
import os
from pathlib import Path
import subprocess
import time


class ModuleError(Exception):
    def __init__(self, code, stage, module=None):
        self.code, self.stage, self.module = code, stage, module
        super().__init__(f'{stage}: Win32 error {code}, module={module}')


def observe_modules(api, attempts=4, delay=.025):
    observations, previous = [], None
    identity = api.identity()
    for attempt in range(1, attempts + 1):
        row = {'attempt': attempt, 'identity': identity}
        if not api.alive():
            row['status'] = 'EXITED'; observations.append(row)
            return {'status': 'EXITED', 'observations': observations, 'identity': identity}
        try:
            names = sorted(set(api.snapshot()))
            if api.identity() != identity:
                row['status'] = 'IDENTITY_CONFLICT'; observations.append(row)
                return {'status': 'ERROR', 'observations': observations, 'identity': identity}
            if not api.alive():
                row['status'] = 'EXITED_AFTER_QUERY'; observations.append(row)
                return {'status': 'EXITED', 'observations': observations, 'identity': identity}
            row.update(status='COMPLETE', modules=names); observations.append(row)
            if names and names == previous:
                return {'status': 'STABLE', 'modules': names, 'observations': observations, 'identity': identity}
            previous = names
        except ModuleError as error:
            # Possible unloading/loader activity, not a claim about the
            # historical failure's exact cause. Access/bitness/unknown errors
            # remain explicit and are never silently retried to success.
            row.update(status='API_ERROR', error=error.code, stage=error.stage, module=error.module, alive=api.alive())
            observations.append(row); previous = None
            if not row['alive']:
                return {'status': 'EXITED', 'observations': observations, 'identity': identity}
            if error.code not in (6, 126, 299, 487):
                return {'status': 'ERROR', 'observations': observations, 'identity': identity}
        if attempt < attempts:
            time.sleep(delay)
    return {'status': 'UNKNOWN', 'observations': observations, 'identity': identity}


class WindowsModules:
    def __init__(self, kernel, handle, identity):
        self.k, self.handle, self._identity = kernel, handle, identity
        self.ps = c.WinDLL('psapi', use_last_error=True)
        self.ps.EnumProcessModules.argtypes = [w.HANDLE, c.POINTER(w.HMODULE), w.DWORD, c.POINTER(w.DWORD)]
        self.ps.GetModuleFileNameExW.argtypes = [w.HANDLE, w.HMODULE, w.LPWSTR, w.DWORD]

    def identity(self):
        return self._identity

    def alive(self):
        result = self.k.WaitForSingleObject(self.handle, 0)
        if result == 0xffffffff:
            raise ModuleError(c.get_last_error(), 'WaitForSingleObject')
        return result == 258

    def snapshot(self):
        capacity = 2048
        while True:
            array = (w.HMODULE * capacity)(); needed = w.DWORD()
            c.set_last_error(0)
            if not self.ps.EnumProcessModules(self.handle, array, c.sizeof(array), c.byref(needed)):
                raise ModuleError(c.get_last_error(), 'EnumProcessModules')
            if needed.value <= c.sizeof(array):
                break
            capacity = needed.value // c.sizeof(w.HMODULE) + 64
            if capacity > 16384:
                raise ModuleError(299, 'module-capacity-limit')
        names = []
        for module in array[:needed.value // c.sizeof(w.HMODULE)]:
            buf = c.create_unicode_buffer(32768); c.set_last_error(0)
            length = self.ps.GetModuleFileNameExW(self.handle, module, buf, len(buf))
            if not length:
                raise ModuleError(c.get_last_error(), 'GetModuleFileNameExW', int(module))
            if length >= len(buf) - 1:
                raise ModuleError(122, 'module-path-truncated', int(module))
            names.append(Path(buf.value).name)
        return names


def cmd_invocation(script, arguments=()):
    """Return an explicit native command string, also from POSIX MSYS Python.

CMD parses /s /c itself; Windows list2cmdline/POSIX exec conversion are not CMD
escaping. Launch a fixed /d /v:off /s /c string; quote every token and validate
the supported literal subset. Percent expansion, quotes and newlines cannot be
made literal by generic batch quoting and are rejected, rather than executed.
Ampersands/parentheses/Unicode/empty arguments remain within double quotes.
"""
    values = [str(script).replace('/', '\\'), *map(str, arguments)]
    if any(any(ch in value for ch in '"%\r\n\x00') for value in values):
        raise ValueError('CMD tokens cannot contain quotes, percent expansion, NUL or newlines')
    # %* forwards the raw tokens from the batch entry to a native executable;
    # double trailing backslashes for its CRT parser before the closing quote.
    def quote(value):
        slashes = len(value) - len(value.rstrip('\\'))
        return '"' + value + '\\' * slashes + '"'
    command = '"' + ' '.join(map(quote, values)) + '"'
    system = os.environ.get('SystemRoot', r'C:\Windows')
    # executable/command are native paths. MSYS2's POSIX Python accepts a
    # sequence; native Python accepts a Windows command string. The command
    # payload is passed verbatim as one argv token on the POSIX boundary.
    # CMD scans its own argv[0] for switches: C:/.../cmd.exe can be parsed
    # starting at /c inside that filename. Use a native backslash path.
    exe = system.replace('/', '\\').rstrip('\\') + '\\System32\\cmd.exe'
    if os.name == 'nt':
        return f'"{exe}" /d /v:off /s /c {command}'
    return [exe, '/d', '/v:off', '/s', '/c', command]


def cmd_environment(environment):
    result = dict(environment)
    # Local child-only conversion control; no global shell/profile changes.
    result['MSYS2_ARG_CONV_EXCL'] = '*'
    result['MSYS_NO_PATHCONV'] = '1'
    return result
