import importlib.util,json,tempfile,unittest,zipfile
from pathlib import Path
S=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('ga01',S/'package-ga01.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Ga01PackagingTests(unittest.TestCase):
 def test_archive_path_escape_is_rejected(self):
  for name in ('../outside','/absolute','runtime/../outside','runtime\\evil','C:/private','a//b','a/./b'):
   with self.subTest(name=name),self.assertRaises(ValueError):m.safe(name)
 def test_unaccepted_input_is_rejected(self):
  with tempfile.TemporaryDirectory() as t:
   p=Path(t)/'different.zip';p.write_bytes(b'not the accepted artifact')
   with self.assertRaisesRegex(ValueError,'frozen accepted'):m.collect(p)
 def test_supplement_hashes_are_enforced(self):
  with tempfile.TemporaryDirectory() as t:
   d=Path(t);(d/'LICENSE.txt').write_bytes(b'original license');(d/'SUPPLEMENT-MANIFEST.json').write_text(json.dumps({'LICENSE.txt':m.digest(b'original license')}))
   self.assertEqual(m.supplement(d),{'licenses/supplemental/LICENSE.txt':b'original license'})
   (d/'LICENSE.txt').write_bytes(b'changed license')
   with self.assertRaisesRegex(ValueError,'manifest mismatch'):m.supplement(d)
 def test_existing_artifact_is_preserved(self):
  with tempfile.TemporaryDirectory() as t:
   z=Path(t)/'private.zip';z.write_bytes(b'preserve')
   with self.assertRaisesRegex(ValueError,'Existing artifact'):m.emit({'runtime/file':b'ok'},z)
   self.assertEqual(z.read_bytes(),b'preserve')
if __name__=='__main__':unittest.main()
