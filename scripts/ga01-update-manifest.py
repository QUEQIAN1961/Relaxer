#!/usr/bin/env python3
"""Explicit owner operation after rebuilding/replacing libraries; never auto-run.

Refuses reparse points. Changes the unsigned corruption-detection baseline;
it does not certify source authenticity, license compliance or release status.
"""
import argparse,hashlib,stat
from pathlib import Path
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--package',type=Path,required=True);p.add_argument('--acknowledge-modified-package',action='store_true');a=p.parse_args()
 if not a.acknowledge_modified_package:p.error('Explicit ownership acknowledgment required')
 root=a.package.resolve();runtime=root/'runtime'
 if not runtime.is_dir():p.error('Missing runtime/')
 lines=[]
 for f in sorted([runtime,*runtime.rglob('*')]):
  if f.is_symlink() or getattr(f.lstat(),'st_file_attributes',0)&stat.FILE_ATTRIBUTE_REPARSE_POINT:raise ValueError('Reparse point refused')
  if f.is_file():
   with f.open('rb') as stream:h=hashlib.file_digest(stream,'sha256').hexdigest()
   lines.append(h+'  '+f.relative_to(root).as_posix()+'\n')
 target=root/'PORTABLE-SHA256SUMS.txt'
 if target.exists():
  backup=root/'PORTABLE-SHA256SUMS.before-owner-modification.txt'
  with backup.open('xb') as out:out.write(target.read_bytes())
 target.write_text(''.join(lines),encoding='utf-8',newline='\n')
 print('Manifest updated for an owner-modified package; official package validation no longer applies.')
if __name__=='__main__':main()
