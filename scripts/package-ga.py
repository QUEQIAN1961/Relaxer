#!/usr/bin/env python3
"""Assemble the GA version from freshly built application PEs and verified dependencies.

Uses the existing portable manifest/launcher protocol. No source preparation,
runtime trimming, cache, new checks or user data migration is introduced.
"""
import argparse,hashlib,json,subprocess,zipfile
from pathlib import Path

BASE_SHA='f33da298e293cfba7783a0965d4152678132e0b24d3861ac7e97f8d7c9a82ded'
def sha(b):return hashlib.sha256(b).hexdigest()
def jb(d):return (json.dumps(d,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
def file_sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def assemble(base,build,source,notice_dir,decision,stage,output):
 if stage.exists() or output.exists():raise ValueError('Use fresh staging/output paths; existing artifacts preserved')
 if file_sha(base)!=BASE_SHA:raise ValueError('Unexpected dependency baseline')
 with zipfile.ZipFile(base) as z:
  names=z.namelist();prefix=names[0].split('/')[0]+'/'
  if z.testzip() is not None:raise ValueError('Baseline CRC failure')
  original={n[len(prefix):]:z.read(n) for n in names if not n.endswith('/')}
  manifest=json.loads(original.pop('PACKAGE-MANIFEST.json'))
  if manifest!={n:sha(b) for n,b in original.items()}:raise ValueError('Baseline manifest mismatch')
 result={n:b for n,b in original.items() if n=='Relaxer.exe' or n.startswith(('runtime/','licenses/','third-party-licenses/')) or n in ['LICENSE','LICENSE-NOTICE.txt']}
 for name,target in [('Relaxer.exe','runtime/Relaxer.exe'),('RelaxerQmlUi.dll','runtime/RelaxerQmlUi.dll'),('RelaxerPortable.exe','Relaxer.exe')]:
  result[target]=(build/name).read_bytes()
 fingerprint=(build/'relaxer-source-fingerprint.txt').read_text().strip()
 marker=('RelaxerBuildSourceSHA256='+fingerprint).encode('ascii')
 for n in ['Relaxer.exe','runtime/Relaxer.exe','runtime/RelaxerQmlUi.dll']:
  if marker not in result[n]:raise ValueError('Compiled source identity mismatch: '+n)
 git=lambda *a:subprocess.check_output(['git',*a],cwd=source).decode().strip()
 identity=dict(sourceCommit=git('rev-parse','HEAD'),sourceTree=git('show','-s','--format=%T','HEAD'),sourceFingerprint=fingerprint,productFrozenCommit='1acde7532ebc34aaa0fa07653798d62cffe64f11',engineeringParent='37c00d330833aa65b0e0152d659e86b65b43487a',displayVersion='V0.2.2-GA',fileVersion='0.2.2.0',entry='Relaxer.exe',published=False,publicReleaseApproved=bool(decision['publicReleaseApproved']),releaseDecision=decision['status'])
 # This is a notice projection only. Original copyright/license bytes survive.
 notice_index=[]
 for p in sorted(notice_dir.rglob('*')):
  if not p.is_file():continue
  rel=p.relative_to(notice_dir).as_posix()
  if rel=='NOTICE-INDEX.json':continue
  if Path(rel).suffix.lower() in ['.exe','.dll','.db','.sqlite','.pfx','.key']:raise ValueError('Invalid notice payload')
  data=p.read_bytes();target='licenses/ga/'+rel
  result[target]=data;notice_index.append(dict(path=target,sha256=sha(data)))
 if (notice_dir/'NOTICE-INDEX.json').exists():result['licenses/ga/NOTICE-INDEX.json']=(notice_dir/'NOTICE-INDEX.json').read_bytes()
 result['THIRD_PARTY_NOTICES.md']=(source/'THIRD_PARTY_NOTICES.md').read_bytes()
 result['READ-ME-FIRST.md']=('''# Relaxer V0.2.2-GA

完整解压到新的可写文件夹，双击根目录 Relaxer.exe。兼容模式：Relaxer.exe --widgets。
程序自带 Qt/QML、FFmpeg 等运行依赖，无需安装开发环境。请保留整个 runtime 和配套清单。

资料默认保存在同级 data/；也可用 --data "绝对资料目录" 指定。升级前完全退出旧版并备份整个 data/，用副本先验证；不要混装运行文件，不要让两个版本同时写同一资料。程序不自动迁移、删除或合并资料。

本地播放可以离线。在线歌词与可选翻译需要网络；凭据和付费请求由用户确认。
自有代码 GPL-3.0-only，第三方保留各自许可。版权与许可证在 licenses/、third-party-licenses/ 和 THIRD_PARTY_NOTICES.md，完整源码和构建资料由同批源码附件提供。

当前公开发行状态记录在 BUILD-IDENTITY.json；以同批最终发行报告和 GitHub 实际 Release 为准。本构建未自动上传发布。
''').encode('utf-8')
 result['BUILD-IDENTITY.json']=jb(dict(**identity,coreBinarySHA256={n:sha(result[n]) for n in ['Relaxer.exe','runtime/Relaxer.exe','runtime/RelaxerQmlUi.dll']}))
 result['deployment.json']=jb(dict(**identity,runtime='runtime/',userDataDefault='data/',userDataOverride='--data or RELAXER_DATA',integrity=dict(manifest='PORTABLE-SHA256SUMS.txt',mode='full-sha256-every-launch',workers=4,cache=False),sourcePreparationRequired=False,readyMarkerRequired=False))
 old=json.loads(original['COMPONENTS.json']);packages=old['packages']
 for p in packages:
  if p['name']=='Relaxer':p['version']=['V0.2.2-GA']
  p.pop('licenseAuditStatus',None)
 binaries=[]
 for x in old['binaries']:
  n=x['path'];b=result[n];binaries.append(dict(path=n,sha256=sha(b),bytes=len(b),component=x['component']))
 result['COMPONENTS.json']=jb(dict(schema=1,displayVersion='V0.2.2-GA',packages=packages,binaries=binaries,sourceMaterials='Relaxer-V0.2.2-GA-Source.zip and Relaxer-V0.2.2-GA-Third-Party-Source.zip',publicReleaseApproved=identity['publicReleaseApproved']))
 # Preserve all 320 third-party PEs and all other runtime files exactly.
 changed=[n for n in result if n.startswith('runtime/') and n not in ['runtime/Relaxer.exe','runtime/RelaxerQmlUi.dll'] and result[n]!=original[n]]
 if changed:raise ValueError('Unexpected dependency modification: '+str(changed))
 result['PORTABLE-SHA256SUMS.txt']=''.join(sha(b)+'  '+n+'\n' for n,b in sorted(result.items()) if n.startswith('runtime/')).encode('utf-8')
 result['PACKAGE-MANIFEST.json']=jb({n:sha(b) for n,b in sorted(result.items())})
 if [n for n in result if '/' not in n and n.lower().endswith('.exe')]!=['Relaxer.exe']:raise ValueError('Invalid ordinary user entry set')
 if any('relaxer-ready.json' in n.lower() or n.startswith('data/') for n in result):raise ValueError('Unexpected preparation/user data')
 stage.mkdir(parents=True)
 for n,b in result.items():
  p=stage/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
 with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for n,b in sorted(result.items()):z.writestr(output.stem+'/'+n,b)
 return dict(files=len(result),runtimeFiles=sum(n.startswith('runtime/') for n in result),PEs=len(binaries),dependenciesByteUnchanged=True,version='V0.2.2-GA',sha256=file_sha(output),source=identity)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['baseline','build','source','notices','decision','stage','output']:p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(json.dumps(assemble(a.baseline,a.build,a.source,a.notices,json.loads(a.decision.read_text(encoding='utf-8')),a.stage,a.output),ensure_ascii=False))
if __name__=='__main__':main()
