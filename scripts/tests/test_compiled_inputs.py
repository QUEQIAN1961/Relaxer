import json, sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from build_inputs import binary_identity, qml_inputs, CORE_BINARIES
from evidence_run import execute


class CompiledInputs(unittest.TestCase):
    def test_wrong_compiled_identity_and_missing_current_launcher_fail(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td); fingerprint='a'*64
            for name in CORE_BINARIES:
                (root/name).write_bytes(b'MZfixture\0RelaxerBuildSourceSHA256='+fingerprint.encode()+b'\0')
            self.assertEqual(set(binary_identity(root,fingerprint)),set(CORE_BINARIES))
            (root/'Relaxer.exe').write_bytes(b'MZold-build')
            with self.assertRaisesRegex(ValueError,'identity mismatch'): binary_identity(root,fingerprint)
            (root/'Relaxer.exe').write_bytes(b'MZfixture\0RelaxerBuildSourceSHA256='+fingerprint.encode()+b'\0')
            (root/'RelaxerStart.exe').unlink()
            with self.assertRaises(FileNotFoundError): binary_identity(root,fingerprint)

    def test_missing_qml_plugin_and_foreign_module_fail(self):
        with tempfile.TemporaryDirectory() as td:
            prefix=Path(td); modules=[]
            for name in ('QtQuick','QtQuick.Controls','QtQuick.Layouts','QtQml'):
                path=prefix/'share/qt6/qml'/name.replace('.','/')
                path.mkdir(parents=True,exist_ok=True); (path/'qmldir').write_text('module '+name)
                modules.append(dict(name=name,type='module',path=str(path)))
            self.assertEqual(len(qml_inputs(modules,[prefix])),4)
            modules[0]['plugin']='missingplugin'
            with self.assertRaisesRegex(ValueError,'Missing QML plugin'):qml_inputs(modules,[prefix])
            modules[0]['path']=str(prefix/'foreign')
            with self.assertRaisesRegex(ValueError,'foreign'):qml_inputs(modules,[prefix])
            with self.assertRaisesRegex(ValueError,'imports absent'):qml_inputs(modules[1:],[prefix])

    def test_timeout_retains_failure_receipt_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as td:
            result=execute([sys.executable,'-c','import time;time.sleep(3)'],td,'timeout',timeout=.05)
            self.assertEqual(result['status'],'ERROR')
            self.assertIn('TimeoutExpired',result['error'])
            self.assertTrue((Path(td)/'timeout.json').is_file())
            with self.assertRaises(FileExistsError):execute([sys.executable,'-c','pass'],td,'timeout')

if __name__=='__main__':unittest.main(verbosity=2)
