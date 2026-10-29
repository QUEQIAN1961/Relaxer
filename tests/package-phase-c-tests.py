"""Policy tests; synthetic filenames are not a substitute for native QA."""
import importlib.util
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("packager", Path(__file__).parents[1] / "scripts/package-phase-c.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class PackagingPolicy(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.stage = self.root / "fresh stage"
        self.stage.mkdir()
        for name in ["RelaxerStart.exe", "RelaxerQml.cmd", "runtime/Relaxer.exe", "runtime/RelaxerQmlUi.dll",
                     "runtime/relaxer-ready.json", "runtime/Qt6Qml.dll", "runtime/Qt6Quick.dll", "runtime/Qt6QuickControls2.dll"]:
            file = self.stage / name
            file.parent.mkdir(exist_ok=True)
            file.write_bytes(b"policy fixture")
        (self.stage / "deployment.json").write_text(json.dumps({"stagingPolicy": "fresh-build-inputs-only", "verified": True,
            "binarySHA256": {name: hashlib.sha256(b"policy fixture").hexdigest()
                             for name in ("Relaxer.exe", "RelaxerStart.exe", "RelaxerQmlUi.dll")}}))

    def test_deterministic_archive(self):
        a, b = self.root / "a.zip", self.root / "b.zip"
        first = packager.package(self.stage, a, "test")
        second = packager.package(self.stage, b, "test")
        self.assertEqual(first, second)
        self.assertEqual(a.read_bytes(), b.read_bytes())

    def test_reject_residue_before_creating_output(self):
        for name in ["runtime/startup.log", "runtime/qt.LOG", "data/keep.txt", "runtime/test.sqlite-wal",
                     "runtime/old.db-shm", "runtime/music.mp3", "runtime/test.mid", "runtime/.env",
                     "runtime/private.key", "runtime/.cache/cache", "runtime/item.qmlc",
                     "runtime/relaxer_qml_c6_evidence_tests.exe", "runtime/Qt6Test.dll"]:
            with self.subTest(name=name):
                file = self.stage / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(b"must remain preserved")
                with self.assertRaises(ValueError):
                    packager.package(self.stage, self.root / "bad.zip", "test")
                self.assertFalse((self.root / "bad.zip").exists())
                self.assertEqual(file.read_bytes(), b"must remain preserved")
                file.unlink()

    def test_unverified_candidate_is_rejected(self):
        (self.stage / "deployment.json").write_text(json.dumps({"stagingPolicy": "fresh-build-inputs-only", "verified": False}))
        with self.assertRaises(ValueError): packager.inventory(self.stage)

    def test_existing_artifact_is_preserved(self):
        output = self.root / "keep.zip"
        output.write_bytes(b"previous artifact")
        with self.assertRaises(ValueError): packager.package(self.stage, output, "test")
        self.assertEqual(output.read_bytes(), b"previous artifact")

    def test_required_module_is_checked(self):
        (self.stage / "runtime/RelaxerQmlUi.dll").unlink()
        with self.assertRaises(ValueError): packager.inventory(self.stage)

    def test_replaced_binary_is_rejected(self):
        (self.stage / "runtime/RelaxerQmlUi.dll").write_bytes(b"unverified replacement")
        with self.assertRaises(ValueError): packager.inventory(self.stage)

    def test_private_test_requires_complete_bound_proof_without_ready_marker(self):
        proof=json.loads((self.stage/'deployment.json').read_text())
        proof.update(verified=False,sourceCommit='commit',sourceTree='tree',sourceFingerprint='fingerprint')
        for name in ('TEST-ONLY-README.txt','BUILD-IDENTITY.json','RelaxerWidgets.cmd','RelaxerLockCheck.exe','runtime/qt.conf'):
            (self.stage/name).write_bytes(b'policy fixture')
        proof['coreBinarySHA256']={**proof['binarySHA256'],'RelaxerLockCheck.exe':hashlib.sha256(b'policy fixture').hexdigest()}
        (self.stage/'deployment.json').write_text(json.dumps(proof))
        qa={key:proof[key] for key in ('sourceCommit','sourceTree','sourceFingerprint','coreBinarySHA256')}
        qa.update(purpose='private-test-unverified',publicDistributionApproved=False,
                  checks={key:'PASS' for key in ('dependencyClosure','qmlStartup','widgetsStartup','sqliteIntegrity')})
        with self.assertRaises(ValueError):packager.inventory(self.stage,qa)
        (self.stage/'runtime/relaxer-ready.json').unlink()
        packager.inventory(self.stage,qa)
        qa['sourceCommit']='another-commit'
        with self.assertRaises(ValueError):packager.inventory(self.stage,qa)
        qa['sourceCommit']=proof['sourceCommit'];qa['checks']['qmlStartup']='FAIL'
        with self.assertRaises(ValueError):packager.inventory(self.stage,qa)


if __name__ == "__main__": unittest.main(verbosity=2)
