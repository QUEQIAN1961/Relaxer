import hashlib, importlib.util, os, subprocess, sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from source_identity import identity
spec=importlib.util.spec_from_file_location('deployment',Path(__file__).resolve().parents[1]/'deploy-phase-c.py')
deployment=importlib.util.module_from_spec(spec);spec.loader.exec_module(deployment)

class ReleaseInputs(unittest.TestCase):
    def test_headers_follow_cmake_policy_and_stale_build_is_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            source=Path(td)/'source with spaces';source.mkdir();build=Path(td)/'build';build.mkdir()
            def git(*args):return subprocess.check_output(['git',*args],cwd=source,stderr=subprocess.STDOUT)
            files={'scripts/source-fingerprint.cmake':b'policy', 'CMakeLists.txt':b'cmake',
                   'src/a.cpp':b'source','src/a.h':b'header','tests/fixture.h':b'test header','tests/a.cpp':b'test'}
            for name,data in files.items():
                p=source/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
            git('init');git('add','.');git('-c','user.name=Fixture','-c','user.email=fixture@local','commit','-m','fixture')
            h=lambda value:hashlib.sha256(value).hexdigest()
            inputs='fingerprint-policy:'+h(files['scripts/source-fingerprint.cmake'])+'\n'+''.join(name+':'+h(data)+'\n' for name,data in sorted(files.items()) if name!='scripts/source-fingerprint.cmake')
            frozen=identity(source);self.assertEqual(frozen['sourceFingerprint'],h(inputs.encode()))
            self.assertEqual(frozen['tree'],git('show','-s','--format=%T','HEAD').decode().strip())
            (build/'CMakeCache.txt').write_text('CMAKE_HOME_DIRECTORY:INTERNAL='+str(source)+'\n',encoding='utf-8')
            (build/'relaxer-source-fingerprint.txt').write_text(frozen['sourceFingerprint']+'\n',encoding='utf-8')
            self.assertEqual(deployment.validate_current_build(source,build,'git'),frozen)
            (build/'relaxer-source-fingerprint.txt').write_text('0'*64,encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'fingerprint'):deployment.validate_current_build(source,build,'git')
            (build/'relaxer-source-fingerprint.txt').write_text(frozen['sourceFingerprint'],encoding='utf-8')
            (build/'CMakeCache.txt').write_text('CMAKE_HOME_DIRECTORY:INTERNAL='+str(build)+'\n',encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'another source'):deployment.validate_current_build(source,build,'git')

if __name__=='__main__':unittest.main(verbosity=2)
