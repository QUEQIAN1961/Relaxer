import importlib.util,tempfile,unittest
from pathlib import Path
script=Path(__file__).resolve().parents[1]/'assemble-ga02-materials.py'
spec=importlib.util.spec_from_file_location('ga02',script);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Ga02MaterialTests(unittest.TestCase):
 def test_wrong_source_digest_is_rejected_without_copy(self):
  with tempfile.TemporaryDirectory() as td:
   r=Path(td);source=r/'source.txt';source.write_bytes(b'tampered');out=r/'out.txt'
   with self.assertRaisesRegex(ValueError,'digest mismatch'):m.copy(source,out,'0'*64)
   self.assertFalse(out.exists())
 def test_existing_output_preserves_prior_delivery(self):
  with tempfile.TemporaryDirectory() as td:
   r=Path(td);out=r/'old-delivery';out.mkdir();marker=out/'preserve';marker.write_bytes(b'old')
   with self.assertRaisesRegex(ValueError,'Existing output'):m.assemble(r/'ga01',r/'ga02',out)
   self.assertEqual(marker.read_bytes(),b'old')
 def test_duplicate_destination_is_not_overwritten(self):
  with tempfile.TemporaryDirectory() as td:
   r=Path(td);a=r/'a';b=r/'b';a.write_bytes(b'new');b.write_bytes(b'old')
   with self.assertRaisesRegex(ValueError,'Duplicate'):m.copy(a,b)
   self.assertEqual(b.read_bytes(),b'old')
if __name__=='__main__':unittest.main()
