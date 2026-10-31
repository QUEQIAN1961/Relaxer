"""Exercise the built launcher with a genuine same-build static child probe.

The probe is RelaxerLockCheck invoked with deliberately invalid arguments: exit
2 proves CreateProcess reached it; launcher integrity failures must be exit 12.
No Qt UI, personal data, ready marker or network access is involved.
"""
import argparse,hashlib,json,os,shutil,subprocess,tempfile
from pathlib import Path

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--launcher',type=Path,required=True);parser.add_argument('--probe',type=Path,required=True);parser.add_argument('--fingerprint',type=Path,required=True);parser.add_argument('--work',type=Path,required=True);a=parser.parse_args()
 a.work.mkdir(parents=True,exist_ok=True)
 fingerprint=a.fingerprint.read_text().strip();marker=b'RelaxerBuildSourceSHA256='+fingerprint.encode()+b'\0'
 results=[]
 with tempfile.TemporaryDirectory(prefix='portable-integrity-',dir=a.work) as temporary:
  root=Path(temporary).resolve();assert root.is_relative_to(a.work.resolve())
  shutil.copy2(a.launcher,root/'Relaxer.exe');runtime=root/'runtime';(runtime/'platforms').mkdir(parents=True)
  shutil.copy2(a.probe,runtime/'Relaxer.exe')
  files={'runtime/RelaxerQmlUi.dll':marker,'runtime/Qt6Core.dll':b'core','runtime/Qt6Quick.dll':b'quick','runtime/platforms/qwindows.dll':b'platform','runtime/qt.conf':b'[Paths]\nPrefix=.\n','runtime/late-resource.bin':b'x'*(1024*1024*8)}
  for name,body in files.items():(root/name).write_bytes(body)
  def manifest():
   return ''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.relative_to(root).as_posix()+'\n' for f in sorted(runtime.rglob('*')) if f.is_file())
  original=manifest();m=root/'PORTABLE-SHA256SUMS.txt';m.write_text(original,encoding='utf-8')
  def launch(name,expected):
   diagnostic=root/(name+'.json');trace=Path(str(diagnostic)+'.launcher.json')
   p=subprocess.run([str(root/'Relaxer.exe'),'--data',str(root/'unused-data'),'--diagnostics',str(diagnostic),'--quit-after','1'],capture_output=True,timeout=30)
   assert p.returncode==expected,(name,p.returncode,p.stderr.decode(errors='replace'))
   if expected==12:assert not trace.exists(),('Integrity failure reached child',name)
   else:
    timing=json.loads(trace.read_text());assert timing['files']==7 and timing['bytes']>8*1024*1024
    assert timing['createdQpcMs']>=timing['verifiedQpcMs']
   assert not (root/'unused-data').exists() and not (runtime/'relaxer-ready.json').exists()
   results.append({'case':name,'status':'PASS','exitCode':p.returncode})
  launch('valid-all-files-before-child',2)
  m.rename(root/'held-manifest');launch('manifest-missing',12);(root/'held-manifest').rename(m)
  f=runtime/'late-resource.bin';body=f.read_bytes();stamp=f.stat();f.write_bytes(body[:-1]+b'y');os.utime(f,ns=(stamp.st_atime_ns,stamp.st_mtime_ns));launch('tail-tamper-same-size-mtime',12);f.write_bytes(body)
  f.rename(runtime/'held-resource');launch('file-missing',12);(runtime/'held-resource').rename(f)
  m.write_text(original+'0'*64+'  runtime/../outside.bin\n',encoding='utf-8');launch('path-traversal',12)
  m.write_text(original+original.splitlines()[0]+'\n',encoding='utf-8');launch('duplicate-entry',12)
  m.write_text(original.replace('runtime/qt.conf','runtime:qt.conf'),encoding='utf-8');launch('invalid-runtime-path',12)
  qml=runtime/'RelaxerQmlUi.dll';qml.write_bytes(marker.replace(fingerprint.encode(),b'0'*64));m.write_text(manifest(),encoding='utf-8');launch('wrong-source-with-updated-sha256',12)
  qml.write_bytes(marker);m.write_text(original,encoding='utf-8');launch('restored-valid',2)
 print(json.dumps({'status':'PASS','fullHashEachLaunch':True,'cases':results},indent=2))

if __name__=='__main__':main()
