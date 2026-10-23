import importlib.util,json,os,subprocess,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ready_marker import marker,read
from evidence_run import execute
from release_gate import evaluate,digest
from source_identity import identity

class Contracts(unittest.TestCase):
    def test_ready_old_new_unknown_fields(self):
        self.assertEqual(read({'version':'1.5.0'})['productVersion'],None)
        self.assertEqual(read(marker('0.2.1',future=True)),{'readySchemaVersion':'1.5.0','productVersion':'0.2.1'})
    def test_ready_conflicts_missing_and_corruption(self):
        for bad in [{},{'version':'0.2.1'},None,{'version':'1.5.0','readySchemaVersion':'2'},{'readySchemaVersion':'1.5.0','productVersion':5}]:
            with self.assertRaises(ValueError):read(bad)
        with self.assertRaises(json.JSONDecodeError):read(json.loads('{corrupt'))
    def test_empty_success_and_controlled_failure_receipts(self):
        with tempfile.TemporaryDirectory() as td:
            good=execute([sys.executable,'-c','pass'],td,'empty',identity={'fixture':True})
            bad=execute([sys.executable,'-c','import sys;print("controlled failure",file=sys.stderr);sys.exit(7)'],td,'failure')
            self.assertEqual(good['status'],'PASS');self.assertEqual(good['streams']['stdout']['bytes'],0)
            self.assertEqual(bad['status'],'FAIL');self.assertEqual(bad['exitCode'],7);self.assertGreater(bad['streams']['stderr']['bytes'],0)
            with self.assertRaises(FileExistsError):execute([sys.executable,'-c','pass'],td,'empty')
    def test_release_gate_synthetic_positive_and_negative(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);source=root/'source';source.mkdir();delivery=root/'delivery';delivery.mkdir()
            def git(*args):return subprocess.check_output(['git',*args],cwd=source,stderr=subprocess.STDOUT)
            git('init');(source/'scripts').mkdir();(source/'scripts/source-fingerprint.cmake').write_text('fixture policy')
            (source/'CMakeLists.txt').write_text('fixture source');git('add','.');git('-c','user.name=Fixture','-c','user.email=fixture@local','commit','-m','fixture')
            frozen=identity(source);(delivery/'BUILD-IDENTITY.json').write_text(json.dumps(frozen));(delivery/'artifact.zip').write_bytes(b'fixture artifact')
            hashes={'artifact.zip':digest(delivery/'artifact.zip')};(delivery/'SHA256SUMS.txt').write_text(hashes['artifact.zip']+'  artifact.zip\n')
            report=root/'report.md';report.write_text('SYNTHETIC TEST ONLY\n'+'\n'.join(frozen.values()))
            claim={'schemaVersion':1,'testOnly':True,'issuer':{'kind':'independent-harness','name':'synthetic fixture'},'identity':frozen,'artifactHashes':hashes,'report':{'path':'report.md','sha256':digest(report)},'verdict':{'productRegression':'PASS','artifactIntegrity':'PASS','processSafety':'SAFE','freezeReadiness':'READY'},'conditions':[]}
            attestation=root/'attestation.json'
            def check():attestation.write_text(json.dumps(claim));return evaluate(source,delivery,attestation,fixture=True)
            self.assertEqual(check()['status'],'TEST_ONLY');self.assertFalse(check()['eligible'])
            self.assertEqual(evaluate(source,delivery)['status'],'PENDING')
            original=claim['identity'];claim['identity']={**frozen,'commit':'0'*40};self.assertEqual(check()['status'],'BLOCKED');claim['identity']=original
            claim['issuer']['kind']='development-self-test';self.assertEqual(check()['status'],'BLOCKED');claim['issuer']['kind']='independent-harness'
            (delivery/'artifact.zip').write_bytes(b'replaced');self.assertEqual(check()['status'],'BLOCKED');(delivery/'artifact.zip').write_bytes(b'fixture artifact')
            report.write_text('changed report');self.assertEqual(check()['status'],'BLOCKED')

if __name__=='__main__':unittest.main(verbosity=2)
