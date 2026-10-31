#!/usr/bin/env python3
"""Assemble selected GA-02 compliance review materials; never a release approval.

Inputs are read-only. A new destination is required. Compiler installations,
Cargo caches, work directories and executable build outputs are not copied.
Official signed MSYS2 *source* archives are preserved with their build inputs.
"""
import argparse,hashlib,json,shutil,stat,urllib.parse
from pathlib import Path
def digest(path):
 with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(path):return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def write(path,data):path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
def regular(path):
 if path.is_symlink() or getattr(path.lstat(),'st_file_attributes',0)&stat.FILE_ATTRIBUTE_REPARSE_POINT:raise ValueError('Reparse/symlink input refused')
def copy(source,target,expected=None):
 regular(source)
 if expected is not None and digest(source)!=expected:raise ValueError('Source digest mismatch: '+source.name)
 target.parent.mkdir(parents=True,exist_ok=True)
 if target.exists():raise ValueError('Duplicate assembly target')
 shutil.copy2(source,target)
 if digest(source)!=digest(target):raise ValueError('Copy digest mismatch')
def tree(source,target):
 if not source.exists():return
 regular(source)
 for p in sorted(source.rglob('*')):
  regular(p)
  if p.is_file():copy(p,target/p.relative_to(source))
def assemble(ga01,ga02,destination):
 ga01,ga02,destination=[Path(p).resolve() for p in (ga01,ga02,destination)]
 if destination.exists():raise ValueError('Existing output preserved')
 if destination.is_relative_to(ga01):raise ValueError('GA-01 is read-only')
 destination.mkdir(parents=True)
 proven=read(ga01/'third-party/source-validation.json')
 for archive,proof in sorted(proven.items()):
  stem=archive.split('.src.tar.')[0]
  copy(ga01/'third-party/source-archives'/archive,destination/'components'/stem/archive,proof['archiveSHA256'])
  copy(ga01/'third-party/source-archives'/(archive+'.sig'),destination/'components'/stem/(archive+'.sig'))
  tree(ga01/'third-party/recipes'/stem,destination/'components'/stem/'recipe')
 for p in (ga01/'third-party/static-source-supplement').iterdir():
  if p.is_file() and not p.name.endswith('.partial'):copy(p,destination/'static-build-source-supplement'/p.name)
 packages=read(ga01/'third-party/component-evidence.json')
 for p in packages:
  directory=ga01/'third-party/package-evidence'/p['component']
  for name in ('.PKGINFO','.BUILDINFO','.MTREE','binary-package.sig','binary-package-reference.json'):
   if (directory/name).exists():copy(directory/name,destination/'provenance/binary-packages'/p['component']/name)
 for name in ('source-map.json','component-evidence.json'):
  copy(ga01/'third-party'/name,destination/'provenance'/name)
 copy(ga01/'third-party/verification-keyring/msys2-public-keys.gpg',destination/'provenance/msys2-public-keys.gpg')
 tree(ga01/'third-party/signature-logs',destination/'provenance/signature-logs')
 for p in read(ga01/'third-party/cargo-source-audit.json')['crates']:
  name=p['name']+'-'+p['version']+'.crate';copy(ga01/'third-party/cargo-crates'/name,destination/'rust/crates'/name,p['expectedSHA256'])
 for p in read(ga02/'evidence/native-replay-added-crates.json'):
  name=p['name']+'-'+p['version']+'.crate';target=destination/'rust/crates'/name
  if not target.exists():copy(ga02/'materials/additional-crates'/name,target,p['sha256'])
  write(destination/'rust/registry-records'/(p['name']+'-'+p['version']+'.json'),p['registryRecord'])
 for src,name in (
  (ga01/'third-party/license-inspection/mingw-w64-librsvg-2.63.2-1-texts/librsvg-2.63.2__Cargo.lock','librsvg-upstream.Cargo.lock'),
  (ga01/'third-party/license-inspection/mingw-w64-rav1e-0.8.1-1-texts/rav1e-0.8.1__Cargo.lock','rav1e-original.Cargo.lock'),
  (ga01/'third-party/license-inspection/mingw-w64-libdovi-3.4.0-1-texts/dovi_tool-libdovi-3.4.0__dolby_vision__Cargo.lock','libdovi-original.Cargo.lock'),
  (ga02/'materials/librsvg/locks/native-registry-replayed-Cargo.lock','librsvg-reconstructed-CI-CORROBORATED.Cargo.lock')):copy(src,destination/'rust/locks'/name)
 # Package notices only for actually used owners, plus their compiler/startup grants.
 names={p['component'].removeprefix('mingw-w64-ucrt-x86_64-') for p in packages}
 names|={'gcc','winpthreads','crt','headers'}
 names|={p['source']['base'].removeprefix('mingw-w64-') for p in packages}
 for name in sorted(names):tree(ga01/'final-engineering-stage/third-party-licenses'/name,destination/'notices/package-notices'/name)
 tree(ga01/'final-engineering-stage/licenses/startup',destination/'notices/compiler-startup')
 tree(ga01/'license-supplement-final/qt-source-attributions',destination/'notices/qt-exact-source-attribution-superset')
 tree(ga01/'third-party/cargo-notices',destination/'notices/original-crate-notices')
 tree(ga02/'materials/recovered-crate-notices',destination/'notices/recovered-crate-notices')
 tree(ga02/'materials/embedded-source-notices',destination/'notices/embedded-source-notices')
 for p in sorted((ga02/'materials/opencc').rglob('*')):
  if p.is_file() and p.name not in ('reproduced-opencc-st.inc','independently-generated.inc'):copy(p,destination/'opencc'/p.relative_to(ga02/'materials/opencc'))
 for name in ('LICENSE','LICENSE-NOTICE.txt'):copy(ga02/'source'/name,destination/'application-source-license'/name)
 tree(ga02/'materials/vcs-source-snapshots',destination/'verified-vcs-archive-snapshots')
 def public_value(value):
  if isinstance(value,dict):return {k:public_value(v) for k,v in value.items()}
  if isinstance(value,list):return [public_value(v) for v in value]
  if isinstance(value,str):
   for root,label in ((ga02,'<GA02>'),(ga01,'<GA01>')):
    for old in (str(root),root.as_posix(),urllib.parse.quote(root.as_posix(),safe='/:'),urllib.parse.quote(str(root))):value=value.replace(old,label)
  return value
 projections=[]
 for name in ('final-binary-dependency-edges.json','static-component-audit.json','embedded-source-review.json','target-crate-graphs.json','crate-traceability-matrix.json','crate-notice-final-triage.json','notice-exact-repo-search.json','loose-resource-ownership.json','qt-compiled-resource-inventory.json','ga01-payload-assertion-correction.json','vcs-payload-checks.json','librsvg-reconstruction.json','librsvg-native-registry-replay.json','librsvg-original-ci-corroboration.json','github-ci-connector-retrieval.json','opencc-source-chain.json','opencc-legal-notice-check.json','opencc-binary-table-verification.json'):
  source=ga02/'evidence'/name;write(destination/'mapping'/name,public_value(read(source)));projections.append(dict(source='evidence/'+name,sourceSHA256=digest(source),publicProjectionSHA256=digest(destination/'mapping'/name),normalization='GA-01/GA-02 absolute local roots replaced by placeholders; original evidence retained in audit archive'))
 write(destination/'mapping/PROJECTION-INPUTS.json',projections)
 write(destination/'provenance/source-validation.json',public_value(read(ga01/'third-party/source-validation.json')))
 tree(ga02/'references/librsvg-ci',destination/'provenance/librsvg-original-ci')
 for name in ('notice-git-tree-0.json','notice-git-tree-1.json','notice-git-tree-2.json'):
  copy(ga02/'references'/name,destination/'provenance/notice-git-trees'/name)
 for name in ('THIRD-PARTY-OBLIGATIONS-GA02.json','SOURCE-PROVISION-AND-REBUILD.md','05-Remaining-Compliance-Issues.md'):
  copy(ga02/name,destination/name)
 write(destination/'STATUS.json',dict(status='GA-02 THIRD-PARTY COMPLIANCE BLOCKED — REMAINING ISSUES DOCUMENTED',publicReleaseApproved=False,productFrozenCommit='1acde7532ebc34aaa0fa07653798d62cffe64f11',ga01EngineeringCommit='049e12ac38c2eb5c124849b7d860b2e9bc515976',originalCILibrsvgLockAuthenticated=False,librsvgDependencyContentCorroborated=True,remainingBlocker='LC-02: static/embedded file-level obligations not completely audited',sourcePackageScope='Exact selected component source archives, patches, recipes, locks, crates, grants and provenance; no entire MSYS2 installation or unrelated work cache',originalVCSArchiveNote='Some official signed source packages contain upstream bare repositories required by their original VCS/version recipes; independent temporary clones and tool caches are excluded',noAdditionalLicenseRestriction='Review status is an engineering decision, not a restriction on rights granted by the included licenses'))
 (destination/'README.md').write_text('''# GA-02 corresponding-source review materials

Read STATUS.json and 05-Remaining-Compliance-Issues.md first. This set is not a
statement that the third-party compliance gate is closed. Original librsvg CI
delta and 177 compiled versions corroborate reconstructed dependency content;
the original post-prepare lockfile bytes were not returned and are not asserted.
SOURCE-PROVISION-AND-REBUILD.md describes recovery and the controlled alternative.
Original package archives preserve preferred source, patches and recipe helpers.
Only the 637 previously locked crates plus 10 exact native-replay additions are
selected; experimental minimal-version candidates and tool installations are absent.
Qt/embedded source notices can include upstream test/example assets; the resource
mapping distinguishes these from files actually present in the runtime.
No restriction on license-granted user rights is imposed by this review label.
''',encoding='utf-8')
 entries={}
 for p in sorted(destination.rglob('*')):
  regular(p)
  if not p.is_file():continue
  rel=p.relative_to(destination).as_posix()
  if any(part in ('.git','__pycache__','target','CMakeFiles') for part in p.relative_to(destination).parts) or p.suffix.lower() in ('.exe','.dll','.obj','.o','.pyc','.sqlite','.db','.pfx','.key') or p.name.endswith('.partial'):raise ValueError('Unrelated build/private residue refused: '+rel)
  entries[rel]=digest(p)
 write(destination/'MATERIALS-SHA256.json',entries)
 return entries
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ga01',type=Path,required=True);p.add_argument('--ga02',type=Path,required=True);p.add_argument('--destination',type=Path,required=True);a=p.parse_args();entries=assemble(a.ga01,a.ga02,a.destination);print(json.dumps(dict(status='ASSEMBLED — REVIEW REQUIRED',files=len(entries))))
