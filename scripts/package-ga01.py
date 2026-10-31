#!/usr/bin/env python3
"""Reassemble a frozen TextFix runtime for GA-01 engineering, never a GA release.

No compiler, download, readiness marker, user database, or original package is
modified. Package metadata is a new projection; original evidence stays outside
the user package. Every runtime byte and the portable manifest are preserved.
"""
import argparse,hashlib,json,re,zipfile,stat
from pathlib import Path,PurePosixPath

BASE_COMMIT='1acde7532ebc34aaa0fa07653798d62cffe64f11'
BASE_TREE='1f999374f49f15e25119b84018607f7a824a6730'
BASE_FP='bc0273477a62f2cf14589ec9041479af0edc6506bb74c6d6cdac05e9236a2427'
BASE_ZIP='b4a6a3922cd3f461142c132a9cd956a843e6b724b23757fa72c1971713b800d1'
LABEL='GA RELEASE-ENGINEERING TEST — NOT FOR PUBLIC DISTRIBUTION'
def digest(data):return hashlib.sha256(data).hexdigest()
def jsonbytes(data):return (json.dumps(data,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
def safe(name):
 p=PurePosixPath(name)
 if not name or p.is_absolute() or any(s in ('','..','.') for s in name.split('/')) or '\\' in name or ':' in name:
  raise ValueError('Unsafe archive path')
 return p
def collect(archive):
 if digest(archive.read_bytes())!=BASE_ZIP:raise ValueError('Input is not the frozen accepted TextFix archive')
 with zipfile.ZipFile(archive) as z:
  if z.testzip() is not None:raise ValueError('Input ZIP CRC failed')
  names=z.namelist()
  if len(names)!=len(set(n.casefold() for n in names)):raise ValueError('Duplicate ZIP member')
  for n in names:safe(n)
  prefix=names[0].split('/')[0]+'/'
  if any(not n.startswith(prefix) for n in names):raise ValueError('Mixed archive roots')
  files={n[len(prefix):]:z.read(n) for n in names}
 manifest=json.loads(files.pop('PACKAGE-MANIFEST.json'))
 if manifest!={n:digest(b) for n,b in files.items()}:raise ValueError('Input manifest mismatch')
 proof=json.loads(files['deployment.json'])
 if (proof['sourceCommit'],proof['sourceTree'],proof['sourceFingerprint'])!=(BASE_COMMIT,BASE_TREE,BASE_FP):raise ValueError('Input identity mismatch')
 if any(n.endswith('relaxer-ready.json') for n in files):raise ValueError('Unexpected preparation marker')
 return files,proof
def project(files,proof):
 keep=lambda n:n=='Relaxer.exe' or n=='PORTABLE-SHA256SUMS.txt' or n in ('LICENSE','LICENSE-NOTICE.txt') or n.startswith(('runtime/','licenses/','third-party-licenses/'))
 result={n:b for n,b in files.items() if keep(n)}
 result['READ-ME-FIRST.md']=('''# Relaxer Windows 便携包 · GA-01 工程验证

GA RELEASE-ENGINEERING TEST — NOT FOR PUBLIC DISTRIBUTION

本包用于发行工程验证，产品版本仍为 V0.2.2-RC7，尚未正式发行。
完整解压到当前用户可写的本地目录，然后双击根目录 Relaxer.exe。
首次打开可能因 Windows 安全扫描多等待数秒；请等待窗口出现，避免重复打开。
不要在 ZIP 预览中运行，不要混用不同版本的 runtime 或清单。

兼容界面可使用 Relaxer.exe --widgets。日常启动不需要源码准备工具、ready 文件或联网。
默认音乐资料与设置保存在包根目录 data/，与 runtime/ 分开。请备份整个 data/。
也可用 Relaxer.exe --data "绝对资料目录"；两种界面应使用同一个资料目录。
已有资料不会自动迁移；换路径时请明确指定原资料目录，勿覆盖原备份。
本便携方案要求包根目录可写，未提供 Program Files 的只读安装方案。

如文件被隔离、丢失或校验失败，请在 Windows 安全中心查看保护历史，核实下载来源，
重新下载并解压完整包到新目录；保留 data/ 备份，不关闭防护，不添加目录排除项。
runtime/ 与 PORTABLE-SHA256SUMS.txt 必须保持配套。安装目录可整体移动。
本入口校验每个清单文件，不维护校验缓存，也不会创建源码准备标记。

GPL-3.0-only 许可适用于 Relaxer 自有代码；第三方组件另有许可，见 THIRD_PARTY_NOTICES.md。
源码、修改与重新构建方法随开发者材料提供。本工程验证包尚未通过全部发行许可与人工验收门。
''').encode('utf-8')
 result['THIRD_PARTY_NOTICES.md']=('''# 第三方组件说明

Relaxer 自有代码采用 GPL-3.0-only。第三方组件保留原版权与许可。
本包动态使用 Qt 6.11.2、Qt Multimedia FFmpeg 后端及其 QML 插件。
FFmpeg/ffprobe 9.0.2 的 MSYS2 构建启用了 GPL 与 version3，应按 GPL-3.0-or-later 审查。
不能将整个运行包统称为 LGPL。MinGW/GCC 运行库的适用例外见 licenses/startup/。
组件版本、二进制 SHA-256 和软件包名见 COMPONENTS.json。
各组件原始许可文本保留在 third-party-licenses/，包括所用软件包之外的原部署附带文本。
OpenCC STCharacters 字表采用 Apache-2.0；其精确来源、归属及对应材料仍在发行审计中。
所有第三方源码、补丁和配方的完整性状态记录在本包之外的 GA-01 发行工程材料中。

源码：Qt https://download.qt.io/ ；FFmpeg https://ffmpeg.org/download.html ；
MSYS2 https://repo.msys2.org/mingw/sources/ 。仅有链接或许可文本不代表源码提供义务已履行。
正式分发前必须提供与二进制版本对应的完整源码和构建资料。

允许依许可证修改、调试和替换共享库。替换与 ABI 兼容的第三方 DLL 后，按源码交付中的
scripts/ga01-update-manifest.py 重新生成清单；此操作应由修改包的拥有者主动执行。
修改 Relaxer 自身时需重建应用、QML DLL 和根启动器，使用同一源码指纹并重建清单。
没有厂商密钥或联网激活要求。本包不设置禁止为调试库修改而逆向工程的附加条款。
音乐、封面、歌词和外部服务内容不属于本软件许可授权范围。
''').encode('utf-8')
 cores={k:v for k,v in proof['coreBinarySHA256'].items() if k not in ('RelaxerStart.exe','RelaxerLockCheck.exe')}
 identity=dict(sourceCommit=BASE_COMMIT,sourceTree=BASE_TREE,sourceFingerprint=BASE_FP,coreBinarySHA256=cores)
 result['BUILD-IDENTITY.json']=jsonbytes(dict(**identity,displayVersion='V0.2.2-RC7',engineeringId='GA-01',targetVersion='V0.2.2-GA',status=LABEL,GA=False,publicDistributionApproved=False,productBytes='frozen TextFix',applicationEntry='Relaxer.exe',preparationEntryIncluded=False))
 result['deployment.json']=jsonbytes(dict(**identity,schema=1,applicationEntry='Relaxer.exe',runtime='runtime/',userDataDefault='data/',userDataOverride='--data or RELAXER_DATA',requiresWritablePackageRoot=True,integrity=dict(manifest='PORTABLE-SHA256SUMS.txt',mode='full-sha256-every-launch',workers=4,cache=False,manifestFiles=1859),sourcePreparationRequired=False,readyMarkerRequired=False,releaseStatus=LABEL,publicDistributionApproved=False,verifiedForPublicRelease=False,provenance=dict(frozenArchiveSHA256=BASE_ZIP)))
 old=json.loads(files['COMPONENTS.json']);used={x['component'] for x in old['binaries'] if x['path'] in result}
 packages=[]
 for p in old['packages']:
  if p['name'] not in used:continue
  row={k:p[k] for k in ('name','version','licenses','upstream') if k in p}
  if p['name']=='Relaxer':row.update(version=['V0.2.2-RC7'],licenses=['GPL-3.0-only'])
  row['licenseAuditStatus']='OPEN — see separate GA-01 audit; no public compliance claim'
  packages.append(row)
 binaries=[{k:x[k] for k in ('path','sha256','bytes','component')} for x in old['binaries'] if x['path'] in result]
 result['COMPONENTS.json']=jsonbytes(dict(schema=1,status=LABEL,legalComplianceClaimed=False,packages=packages,binaries=binaries))
 assert {n:b for n,b in result.items() if n.startswith('runtime/')}=={n:b for n,b in files.items() if n.startswith('runtime/')}
 assert result['PORTABLE-SHA256SUMS.txt']==files['PORTABLE-SHA256SUMS.txt']
 assert len(result['PORTABLE-SHA256SUMS.txt'].splitlines())==1859
 assert [n for n in result if '/' not in n and n.lower().endswith('.exe')]==['Relaxer.exe']
 return result
def emit(files,output):
 if output.exists():raise ValueError('Existing artifact preserved')
 with zipfile.ZipFile(output,'w') as z:
  manifest={n:digest(b) for n,b in sorted(files.items())}
  for n,b in {**files,'PACKAGE-MANIFEST.json':jsonbytes(manifest)}.items():
   safe(n);info=zipfile.ZipInfo(output.stem+'/'+n,(1980,1,1,0,0,0));info.external_attr=0o100644<<16
   z.writestr(info,b,compress_type=zipfile.ZIP_DEFLATED,compresslevel=6)
 return manifest
def supplement(directory):
 for p in [directory,*directory.rglob('*')]:
  if p.is_symlink() or getattr(p.lstat(),'st_file_attributes',0)&stat.FILE_ATTRIBUTE_REPARSE_POINT:raise ValueError('License supplement reparse point refused')
 manifest=json.loads((directory/'SUPPLEMENT-MANIFEST.json').read_text(encoding='utf-8'))
 actual={p.relative_to(directory).as_posix():p.read_bytes() for p in directory.rglob('*') if p.is_file() and p.name!='SUPPLEMENT-MANIFEST.json'}
 if manifest!={n:digest(b) for n,b in actual.items()}:raise ValueError('License supplement manifest mismatch')
 for n in actual:
  safe(n)
  if Path(n).suffix.lower() in ('.exe','.dll','.db','.sqlite','.pfx','.key'):raise ValueError('Non-license payload refused')
 return {'licenses/supplemental/'+n:b for n,b in actual.items()}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--frozen-zip',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--stage',type=Path,required=True)
 p.add_argument('--license-supplement',type=Path)
 a=p.parse_args()
 if 'GA01' not in a.output.name or 'ENGINEERING-TEST-NOT-PUBLIC' not in a.output.name:raise ValueError('Explicit engineering-only filename required')
 files,proof=collect(a.frozen_zip);projected=project(files,proof)
 if a.license_supplement:projected.update(supplement(a.license_supplement))
 if a.stage.exists():raise ValueError('Use a fresh stage')
 a.stage.mkdir(parents=True)
 for n,b in projected.items():f=a.stage/n;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(b)
 manifest=emit(projected,a.output)
 print(json.dumps(dict(status='COLLECTED — native verification pending',files=len(manifest),runtimeUnchanged=True,preparationEntryIncluded=False,zipSHA256=digest(a.output.read_bytes()))))
if __name__=='__main__':main()
