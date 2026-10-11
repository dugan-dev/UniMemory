"""Use one inherited CPU set, preserving capacity for multi-thread workloads."""
import ctypes
import os
from pathlib import Path
import platform
import subprocess
import uuid


def cpu_model():
    cpu = platform.processor()
    if platform.system() == 'Linux':
        for line in Path('/proc/cpuinfo').read_text().splitlines():
            if line.startswith('model name'):
                return line.split(':', 1)[1].strip()
    elif platform.system() == 'Darwin':
        return subprocess.check_output(['sysctl', '-n', 'machdep.cpu.brand_string'], text=True).strip()
    return cpu


def control_host():
    affinity = dict(method='Platform has no supported hard affinity API; scheduler unrestricted',
                    cpus=[], pinned=False)
    if hasattr(os, 'sched_getaffinity'):
        allowed = sorted(os.sched_getaffinity(0))
        cores = {}
        for cpu in allowed:
            topology = Path(f'/sys/devices/system/cpu/cpu{cpu}/topology')
            try:
                key = ((topology / 'physical_package_id').read_text().strip(),
                       (topology / 'core_id').read_text().strip())
                if any(int(value) < 0 for value in key):
                    cores = {}
                    break
            except (OSError, ValueError):
                cores = {}
                break
            cores.setdefault(key, cpu)
        cpus = sorted(cores.values()) if cores else allowed
        os.sched_setaffinity(0, cpus)
        affinity = dict(method='One allowed logical CPU per physical core; worker threads inherit the entire selected core set'
                        if cores else 'Inherited allowed CPU set; physical topology unavailable',
                        cpus=cpus, pinned=True)
    elif os.name == 'nt':
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.GetCurrentProcess.restype = ctypes.c_void_p
        kernel.GetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]
        kernel.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        process = kernel.GetCurrentProcess()
        mask, system = ctypes.c_size_t(), ctypes.c_size_t()
        if not kernel.GetProcessAffinityMask(process, ctypes.byref(mask), ctypes.byref(system)):
            raise ctypes.WinError(ctypes.get_last_error())
        if not kernel.SetProcessAffinityMask(process, mask.value):
            raise ctypes.WinError(ctypes.get_last_error())
        affinity = dict(method='Inherited process affinity mask; normal priority and default QoS',
                        cpus=[index for index in range(mask.value.bit_length()) if mask.value & (1 << index)],
                        pinned=True)
    return dict(host_id=str(uuid.uuid4()), cpu_model=cpu_model(), platform=platform.platform(),
                architecture=platform.machine(), logical_cpus=os.cpu_count(),
                effective_cpus=len(affinity['cpus']) if affinity['pinned'] else os.cpu_count(), affinity=affinity)
