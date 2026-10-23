import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import unittest
from launcher_observation import ModuleError, observe_modules, cmd_invocation


class Fake:
    def __init__(self, snapshots, live=True, conflict=False):
        self.snapshots=iter(snapshots); self.live=live; self.conflict=conflict; self.identities=0
    def identity(self):
        self.identities+=1
        return {'pid':4,'created':20 if not self.conflict or self.identities==1 else 21}
    def alive(self): return self.live
    def snapshot(self):
        value=next(self.snapshots)
        if isinstance(value,Exception): raise value
        if value=='exit': self.live=False; return ['runtime.exe']
        return value


class Observations(unittest.TestCase):
    def observe(self, fake, attempts=4): return observe_modules(fake,attempts,delay=0)
    def test_transient_recovery_requires_two_complete_snapshots(self):
        r=self.observe(Fake([ModuleError(299,'filename'),['runtime.exe'],['runtime.exe']]))
        self.assertEqual(r['status'],'STABLE'); self.assertEqual(len(r['observations']),3)
        self.assertEqual(r['observations'][0]['error'],299)
    def test_module_unloading_reenumerates(self):
        r=self.observe(Fake([['runtime.exe','unloading.dll'],['runtime.exe'],['runtime.exe']]))
        self.assertEqual(r['modules'],['runtime.exe']); self.assertEqual(len(r['observations']),3)
    def test_already_exited_has_no_isolation_evidence(self):
        r=self.observe(Fake([],False)); self.assertEqual(r['status'],'EXITED'); self.assertNotIn('modules',r)
    def test_exit_during_query_is_not_stable(self):
        self.assertEqual(self.observe(Fake(['exit']))['status'],'EXITED')
    def test_access_denied_is_error_not_empty_pass(self):
        r=self.observe(Fake([ModuleError(5,'filename')]))
        self.assertEqual(r['status'],'ERROR'); self.assertEqual(r['observations'][0]['error'],5)
    def test_persistent_transient_is_unknown(self):
        self.assertEqual(self.observe(Fake([ModuleError(299,'filename')]*4))['status'],'UNKNOWN')
    def test_identity_conflict_is_error(self):
        self.assertEqual(self.observe(Fake([['runtime.exe']],conflict=True))['status'],'ERROR')
    def test_empty_live_snapshots_cannot_prove_isolation(self):
        self.assertEqual(self.observe(Fake([[]]*4))['status'],'UNKNOWN')
    def test_nontransient_error_with_live_process_is_error(self):
        self.assertEqual(self.observe(Fake([ModuleError(0,'filename')]))['status'],'ERROR')
    def test_cmd_rejects_expansion_and_injection(self):
        for value in ['%PATH%','a"&calc','a\nb']:
            with self.assertRaises(ValueError): cmd_invocation('safe.cmd',[value])

if __name__=='__main__': unittest.main(verbosity=2)
