import importlib.util
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('causal',ROOT/'tools/ci/diagnose-performance.py')
causal=importlib.util.module_from_spec(spec)
spec.loader.exec_module(causal)


class DiagnosticTests(unittest.TestCase):
    def sample(self,value=10,operations=100):
        return dict(suite='hotpath',backend='mimalloc',variant='native',bytes=64,threads=1,
                    operations=operations,seconds=.1,value=value,unit='ns/pair',checksum=7)

    def test_header_and_empty_output_rejected(self):
        for output in ('',','.join(causal.COLUMNS)+'\n','wrong\n1\n'):
            with self.assertRaises(ValueError): causal.parse_output(output)

    def test_missing_field_and_nonfinite_rejected(self):
        header=','.join(causal.COLUMNS)+'\n'
        for row in ('hotpath,mimalloc,native,64,1,100,.1,nan,ns/pair,7\n',
                    'hotpath,mimalloc,native,64,1,100,.1,10,ns/pair\n',
                    'hotpath,mimalloc,native,64,1,100,0,10,ns/pair,7\n'):
            with self.assertRaises(ValueError): causal.parse_output(header+row)

    def test_unequal_work_rejected(self):
        with self.assertRaisesRegex(ValueError,'Unequal paired work'):
            causal.validate_pair(self.sample(),self.sample(operations=101))

    def test_rss_snapshot_does_not_require_timing(self):
        data=','.join(causal.COLUMNS)+'\nbackend,mimalloc,rss,65536,1,1024,0,67108864,bytes,0\n'
        row=causal.parse_output(data)[0]
        self.assertEqual(row['value'],67108864)

    def test_content_mismatch_rejected(self):
        control=self.sample(); control['suite']='api'
        changed=dict(control,checksum=8)
        with self.assertRaisesRegex(ValueError,'Unequal paired content'):
            causal.validate_pair(control,changed)

    def test_different_backend_or_contract_rejected(self):
        for key,value in (('backend','jemalloc'),('bytes',256),('threads',2),('unit','bytes')):
            changed=self.sample(); changed[key]=value
            with self.assertRaises(ValueError): causal.validate_pair(self.sample(),changed)

    def test_paired_deltas_and_variance_retained(self):
        case=causal.specimen('hotpath','mimalloc','native')
        summary=causal.summary_row('factor','static',(case,case),
                                  [self.sample(10),self.sample(20),self.sample(30)],
                                  [self.sample(12),self.sample(19),self.sample(35)])
        self.assertEqual(summary['paired_delta_median'],2)
        self.assertEqual(summary['paired_delta_min'],-1)
        self.assertEqual(summary['paired_delta_max'],5)
        self.assertFalse(summary['consistent_direction'])

    def test_all_core_factors_are_present(self):
        factors={factor for factor,_,_ in causal.contrasts('single')}
        for factor in ('indirect_dispatch','external_checked_boundary','inline_guards',
                       'actual_stats','atomic_local_cost','cross_worker_sharing',
                       'counter_field_padding','peak_maintenance','global_lookup','cpu_affinity_api'):
            self.assertIn(factor,factors)
        self.assertIn('alignment_bit_conversion',factors)
        self.assertEqual(len([f for f in factors if f.startswith('api_')]),9)


if __name__=='__main__': unittest.main()
