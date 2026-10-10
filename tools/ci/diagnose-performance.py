"""Run randomized controlled probes; never publish their results as benchmarks."""
import argparse
import csv
from collections import defaultdict
import hashlib
import io
import json
import math
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import time

BACKENDS = ('standard', 'mimalloc', 'jemalloc')
HOT = ('native', 'native_constant', 'native_unaligned', 'adapter_direct', 'adapter_pointer',
       'handle_inline_unchecked', 'handle_inline_checked', 'handle_external_unchecked',
       'handle_external_checked', 'handle_checked_constant', 'native_runtime_alignment',
       'handle_runtime_alignment', 'api_cached', 'api_lookup', 'native_batch', 'api_batch')
STATS = ('native', 'api_disabled', 'api_basic', 'synthetic_none',
         'synthetic_thread_local_plain', 'synthetic_shared_relaxed', 'synthetic_shared_padded',
         'synthetic_worker_padded_atomic', 'synthetic_worker_padded_plain',
         'synthetic_shared_no_peak', 'synthetic_shared_live_only', 'synthetic_shared_counts_only')
STATS += ('synthetic_shared_peak_stress','synthetic_shared_peak_stress_no_peak')
API = ('zeroed', 'reallocate', 'reallocate_zeroed', 'unique', 'array', 'shared', 'vector', 'pmr_vector', 'owned')
COLUMNS = ('suite','backend','variant','bytes','threads','operations','seconds','value','unit','checksum')


def parse_output(output):
    reader = csv.DictReader(io.StringIO(output))
    if tuple(reader.fieldnames or ()) != COLUMNS:
        raise ValueError('Invalid diagnostic CSV header')
    rows = list(reader)
    if not rows:
        raise ValueError('Empty diagnostic result')
    seen = set()
    for row in rows:
        if None in row or any(value is None for value in row.values()):
            raise ValueError('Malformed diagnostic row')
        key = row['suite'], row['backend'], row['variant'], row['unit']
        if key in seen:
            raise ValueError('Duplicate diagnostic metric')
        seen.add(key)
        for name in ('bytes','threads','operations','checksum'):
            value = int(row[name])
            if value < 0:
                raise ValueError('Negative diagnostic count')
            row[name] = value
        row['seconds'] = float(row['seconds'])
        if not math.isfinite(row['seconds']) or row['seconds'] < 0:
            raise ValueError('Invalid elapsed duration')
        if row['unit'] != 'text':
            row['value'] = float(row['value'])
            if not math.isfinite(row['value']):
                raise ValueError('Nonfinite metric')
        if row['operations'] and row['unit'].startswith('ns/') and row['seconds'] <= 0:
            raise ValueError('Missing work duration')
    return rows


def validate_pair(control, changed, allow_work_change=False):
    if control['unit'] != changed['unit'] or control['backend'] != changed['backend']:
        raise ValueError('Incompatible paired metrics')
    for key in ('bytes','threads'):
        if control[key] != changed[key]:
            raise ValueError('Unequal paired inputs')
    if not allow_work_change and control['operations'] != changed['operations']:
        raise ValueError('Unequal paired work')
    if (control['suite'].startswith('statistics') or control['suite']=='api') and control['checksum']!=changed['checksum']:
        raise ValueError('Unequal paired content')
    if control['value'] <= 0 or changed['value'] <= 0:
        raise ValueError('Nonpositive comparison metric')


def write_csv(path, rows):
    fields = list(dict.fromkeys(field for row in rows for field in row))
    with path.open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fields)
        writer.writeheader()
        writer.writerows(rows)


def specimen(suite, backend, variant, threads=1, size=64, touch=False, affinity='none'):
    return dict(suite=suite, backend=backend, variant=variant, threads=threads,
                bytes=size, touch=touch, affinity=affinity)


def execute(executable, case, iterations, raw_directory, identity):
    command = [str(executable)]
    for name in ('suite','backend','variant','threads','bytes','affinity'):
        command.extend(['--'+name, str(case[name])])
    command.extend(['--iterations',str(iterations),'--touch','1' if case['touch'] else '0'])
    process = subprocess.run(command, text=True, capture_output=True, timeout=180)
    if raw_directory:
        (raw_directory / (identity+'.csv')).write_text(process.stdout, encoding='utf-8')
        (raw_directory / (identity+'.stderr')).write_text(process.stderr, encoding='utf-8')
    if process.returncode:
        raise RuntimeError(f'Diagnostic command failed {command}: {process.stderr}')
    rows = parse_output(process.stdout)
    for row in rows:
        if case['suite'] != 'clock' and row['backend'] != case['backend']:
            raise ValueError('Unexpected result backend')
    return rows, process.stderr


def primary(rows):
    timed = [row for row in rows if row['operations'] and row['unit'].startswith('ns/')]
    if len(timed) != 1:
        raise ValueError('Expected one primary kernel metric')
    return timed[0]


def contrasts(affinity):
    result=[]
    def add(factor, control, changed):
        result.append((factor, control, changed))
    for b in BACKENDS:
        def hot(v, size=64, touch=False, pin=affinity):
            return specimen('hotpath',b,v,size=size,touch=touch,affinity=pin)
        for factor,a,z in (
            ('whole_wrapper','native','api_cached'),
            ('external_adapter','native','adapter_direct'),
            ('indirect_dispatch','adapter_direct','adapter_pointer'),
            ('external_checked_boundary','handle_inline_checked','handle_external_checked'),
            ('external_unchecked_boundary','handle_inline_unchecked','handle_external_unchecked'),
            ('inline_guards','handle_inline_unchecked','handle_inline_checked'),
            ('external_guards','handle_external_unchecked','handle_external_checked'),
            ('native_constant_size','native','native_constant'),
            ('handle_constant_size','handle_inline_checked','handle_checked_constant'),
            ('native_runtime_alignment','native','native_runtime_alignment'),
            ('handle_runtime_alignment','handle_inline_checked','handle_runtime_alignment'),
            ('global_lookup','api_cached','api_lookup'),
            ('alignment_contract','native_unaligned','native'),
            ('allocation_lifetime_native','native','native_batch'),
            ('allocation_lifetime_api','api_cached','api_batch'),
        ):
            add(factor,hot(a),hot(z))
        if b=='standard':
            add('throwing_native_entry',hot('native'),hot('native_throwing'))
        if b=='jemalloc':
            add('alignment_bit_conversion',hot('adapter_direct'),hot('adapter_cpp20_bits'))
        for size in (16,256,4096,65536):
            add('whole_wrapper',hot('native',size),hot('api_cached',size))
        for size in (64,65536):
            add('memory_touch_native',hot('native',size),hot('native',size,True))
            add('memory_touch_api',hot('api_cached',size),hot('api_cached',size,True))
        if affinity=='single':
            add('cpu_affinity_native',hot('native',pin='none'),hot('native'))
            add('cpu_affinity_api',hot('api_cached',pin='none'),hot('api_cached'))
        for threads in (1,2,4,8,16):
            def counter(v):
                return specimen('statistics',b,v,threads=threads)
            for factor,a,z in (
                ('actual_stats','api_disabled','api_basic'),
                ('stats_kernel_wrapper','native','api_disabled'),
                ('shared_counter_total','synthetic_none','synthetic_shared_relaxed'),
                ('counter_field_padding','synthetic_shared_relaxed','synthetic_shared_padded'),
                ('cross_worker_sharing','synthetic_worker_padded_atomic','synthetic_shared_padded'),
                ('atomic_local_cost','synthetic_worker_padded_plain','synthetic_worker_padded_atomic'),
                ('peak_maintenance','synthetic_shared_no_peak','synthetic_shared_relaxed'),
                ('peak_stress_synthetic','synthetic_shared_peak_stress_no_peak','synthetic_shared_peak_stress'),
                ('count_updates','synthetic_shared_live_only','synthetic_shared_no_peak'),
                ('live_updates','synthetic_shared_counts_only','synthetic_shared_no_peak'),
            ):
                add(factor,counter(a),counter(z))
        for api in API:
            add('api_'+api,specimen('api',b,'native_'+api,affinity=affinity),
                specimen('api',b,'api_'+api,affinity=affinity))
    return result


def smoke(executable, directory, affinity):
    rows=[]
    for b in BACKENDS:
        variants=HOT+(('native_throwing',) if b=='standard' else ())+(('adapter_cpp20_bits',) if b=='jemalloc' else ())
        for v in variants:
            cases=[specimen('hotpath',b,v,affinity=affinity)]
            cases += [specimen('hotpath',b,v,size=65536,touch=True)] if v in ('native','api_cached') else []
            for case in cases:
                value,_=execute(executable,case,257,None,'smoke')
                rows.extend(value)
        for v in STATS:
            case=specimen('statistics',b,v,threads=2)
            value,_=execute(executable,case,17,None,'smoke')
            rows.extend(value)
        for v in API:
            for prefix in ('native_','api_'):
                value,_=execute(executable,specimen('api',b,prefix+v,affinity=affinity),257,None,'smoke')
                rows.extend(value)
    for b in ('mimalloc','jemalloc'):
        variants=['native_heap','api_heap','native_global','api_global']
        if b=='jemalloc': variants+=['native_no_tcache','native_stats','api_stats','epoch_only','native_cached_stats']
        for v in variants:
            value,_=execute(executable,specimen('backend',b,v),256,None,'smoke')
            rows.extend(value)
    write_csv(directory/'smoke.csv',rows)


def calibrate(executable, pair, target):
    iterations=10000
    for _ in range(5):
        times=[primary(execute(executable,case,iterations,None,'calibration')[0])['seconds'] for case in pair]
        fastest=min(times)
        if fastest>=target or iterations>=50000000:
            return iterations
        iterations=min(50000000,max(iterations+1,math.ceil(iterations*target/max(fastest,1e-6)*1.15)))
    return iterations


def summary_row(factor, build, pair, controls, changes):
    for a,z in zip(controls,changes):
        validate_pair(a,z)
    differences=[z['value']-a['value'] for a,z in zip(controls,changes)]
    ratios=[a['value']/z['value']*100 for a,z in zip(controls,changes)]
    med=statistics.median
    return dict(factor=factor,build=build,backend=pair[0]['backend'],bytes=controls[0]['bytes'],
                threads=controls[0]['threads'],control=pair[0]['variant'],changed=pair[1]['variant'],
                control_touch=pair[0]['touch'],changed_touch=pair[1]['touch'],
                control_affinity=pair[0]['affinity'],changed_affinity=pair[1]['affinity'],
                unit=controls[0]['unit'],control_median=med(a['value'] for a in controls),
                changed_median=med(z['value'] for z in changes),
                paired_delta_median=med(differences),paired_delta_min=min(differences),paired_delta_max=max(differences),
                performance_percent=med(ratios),performance_min=min(ratios),performance_max=max(ratios),
                control_seconds_min=min(a['seconds'] for a in controls),changed_seconds_min=min(z['seconds'] for z in changes),
                trials=len(controls),consistent_direction=all(v>0 for v in differences) or all(v<0 for v in differences))


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--builds',required=True,help='JSON mapping build name to diagnostic executable')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--trials',type=int,default=5)
    parser.add_argument('--target-seconds',type=float,default=.1)
    parser.add_argument('--smoke-only',action='store_true')
    args=parser.parse_args()
    if args.trials<5 or args.target_seconds<.1:
        parser.error('At least five trials and a 100ms calibration target are required')
    executables={name:Path(path).resolve() for name,path in json.loads(args.builds).items()}
    out=args.output
    out.mkdir(parents=True,exist_ok=True)
    raw=out/'raw'; raw.mkdir(exist_ok=True)
    affinity='single' if platform.system() in ('Linux','Windows') else 'none'
    allowed=sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else None
    manifest=dict(platform=platform.platform(),machine=platform.machine(),logical_cpus=os.cpu_count(),
                  allowed_cpus=allowed,affinity_supported=affinity=='single',trials=args.trials,
                  target_seconds=args.target_seconds,source_revision=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                  executables={name:dict(path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest()) for name,path in executables.items()},
                  cases_randomized=True,process_isolated=True,paired_order_alternated=True,
                  stats_timing='earliest worker loop start to latest loop finish; launch, warmup and join excluded; staggered scheduling included',
                  caveats=['Synthetic counters change snapshot semantics','Mac CPU affinity unsupported',
                           'Oversubscribed threads include scheduling effects','Aligned vs unaligned changes guarantees',
                           'Signed paired deltas are contextual and not additive'])
    for name,exe in executables.items():
        folder=exe.parent.parent if exe.parent.name=='Release' else exe.parent
        manifest['executables'][name]['build_parameters']=(folder/'diagnostic-build-Release.txt').read_text()
        manifest['executables'][name]['cmake_cache_sha256']=hashlib.sha256((folder/'CMakeCache.txt').read_bytes()).hexdigest()
        if platform.system()=='Linux':
            manifest['executables'][name]['linked_libraries']=subprocess.check_output(['ldd',str(exe)],text=True)
        elif platform.system()=='Darwin':
            manifest['executables'][name]['linked_libraries']=subprocess.check_output(['otool','-L',str(exe)],text=True)
        else:
            manifest['executables'][name]['dlls']={path.name:hashlib.sha256(path.read_bytes()).hexdigest() for path in exe.parent.glob('*.dll')}
    if platform.system()=='Linux':
        manifest['cpu_info']=Path('/proc/cpuinfo').read_text().split('\n\n')[0]
    elif platform.system()=='Darwin':
        manifest['cpu_info']=subprocess.check_output(['sysctl','-n','machdep.cpu.brand_string'],text=True).strip()
    else:
        manifest['cpu_info']=platform.processor()
    if Path('/sys/fs/cgroup/cpu.max').exists():
        manifest['cpu_quota']=Path('/sys/fs/cgroup/cpu.max').read_text().strip()
    (out/'environment.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for name,exe in executables.items():
        smoke_dir=out/name; smoke_dir.mkdir(exist_ok=True)
        smoke(exe,smoke_dir,affinity)
    if args.smoke_only:
        return
    baseline=executables['static']
    cases=contrasts(affinity)
    random.Random(61927).shuffle(cases)
    summaries=[]; measurements=[]
    for index,(factor,a,z) in enumerate(cases):
        pair=(a,z)
        iterations=calibrate(baseline,pair,args.target_seconds)
        controls=[]; changes=[]
        for trial in range(args.trials):
            order=[0,1] if (trial+index)%2==0 else [1,0]
            results={}
            for side in order:
                identity=f'base-{index:03d}-{trial+1}-{side}'
                rows,stderr=execute(baseline,pair[side],iterations,raw,identity)
                value=primary(rows)
                results[side]=value
                measurements.append(dict(build='static',factor=factor,contrast=index,trial=trial+1,side=side,
                                         touch=pair[side]['touch'],affinity=pair[side]['affinity'],
                                         affinity_failed='AFFINITY_UNSUPPORTED' in stderr,**value))
            controls.append(results[0]); changes.append(results[1])
        summaries.append(summary_row(factor,'static',pair,controls,changes))
        write_csv(out/'measurements.csv',measurements)
        write_csv(out/'contrasts.csv',summaries)
        print(f'{index+1}/{len(cases)} {factor} {a["backend"]} {a["bytes"]}B {a["threads"]}t: '
              f'{summaries[-1]["paired_delta_median"]:.3f} {summaries[-1]["unit"]}',flush=True)
    # Build contrasts are paired directly on the same host, not inferred from separate runs.
    for build,exe in executables.items():
        if build=='static': continue
        for b in BACKENDS:
            variants=('native','api_cached','handle_inline_checked','handle_checked_constant')
            if b=='jemalloc': variants+=('adapter_direct','adapter_cpp20_bits')
            for v in variants:
                case=specimen('hotpath',b,v,affinity=affinity)
                n=max(calibrate(baseline,(case,case),args.target_seconds),calibrate(exe,(case,case),args.target_seconds))
                controls=[]; changes=[]
                for trial in range(args.trials):
                    order=[('static',baseline), (build,exe)] if trial%2==0 else [(build,exe),('static',baseline)]
                    for name,program in order:
                        rows,stderr=execute(program,case,n,raw,f'build-{build}-{b}-{v}-{trial+1}-{name}')
                        value=primary(rows)
                        (controls if name=='static' else changes).append(value)
                        measurements.append(dict(build=name,factor='build_'+build,trial=trial+1,**value))
                summaries.append(summary_row('build_'+build,build,(case,case),controls,changes))
        if build=='static-strict-adapters':
            pair=(specimen('hotpath','jemalloc','adapter_direct',affinity=affinity),
                  specimen('hotpath','jemalloc','adapter_cpp20_bits',affinity=affinity))
            n=calibrate(exe,pair,args.target_seconds)
            control=[]; changed=[]
            for trial in range(args.trials):
                for side in ([0,1] if trial%2==0 else [1,0]):
                    row=primary(execute(exe,pair[side],n,raw,f'strict-bits-{trial+1}-{side}')[0])
                    (control if side==0 else changed).append(row)
                    measurements.append(dict(build=build,factor='strict_alignment_bit_conversion',trial=trial+1,side=side,**row))
            summaries.append(summary_row('strict_alignment_bit_conversion',build,pair,control,changed))
    # Backend queries have multiple stage metrics and fixed 64MiB payloads.
    backend_rows=[]
    for b in ('mimalloc','jemalloc'):
        variants=['native_heap','api_heap','native_global','api_global']
        if b=='jemalloc': variants+=['native_no_tcache','native_stats','api_stats','epoch_only','native_cached_stats']
        for trial in range(args.trials):
            shuffled=variants[:]; random.Random(7193+trial).shuffle(shuffled)
            for v in shuffled:
                n=4096 if 'stats' in v or v=='epoch_only' else 20000
                rows,_=execute(baseline,specimen('backend',b,v),n,raw,f'backend-{b}-{v}-{trial+1}')
                backend_rows.extend(dict(trial=trial+1,**row) for row in rows)
    write_csv(out/'backend.csv',backend_rows)
    write_csv(out/'measurements.csv',measurements)
    write_csv(out/'contrasts.csv',summaries)
    clocks=[]
    for trial in range(args.trials):
        rows,_=execute(baseline,specimen('clock','none','clock',affinity=affinity),100000,raw,f'clock-{trial+1}')
        clocks.extend(dict(trial=trial+1,**row) for row in rows)
    write_csv(out/'clock.csv',clocks)
    short=[]
    for b in BACKENDS:
        for trial in range(15):
            for v in ('native','api_cached'):
                row=primary(execute(baseline,specimen('hotpath',b,v,affinity=affinity),4096,raw,f'short-{b}-{v}-{trial+1}')[0])
                short.append(dict(trial=trial+1,**row))
    write_csv(out/'short-duration.csv',short)
    manifest['completed']=True
    manifest['contrast_count']=len(summaries)
    manifest['measurement_rows']=len(measurements)
    (out/'environment.json').write_text(json.dumps(manifest,indent=2)+'\n')


if __name__=='__main__':
    main()
