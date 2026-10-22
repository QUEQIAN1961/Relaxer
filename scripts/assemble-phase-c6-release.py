#!/usr/bin/env python3
"""Assemble a frozen C6 RC1 from fresh verified staging, with independent checks.

All paths are explicit. Never overwrite an archive, run against user data, or
package the executed QA runtime. Evidence inputs are selected through a JSON
list of paths; profiles, media, databases, credentials and runtime binaries are
excluded from the evidence archive.
"""
import argparse
from datetime import datetime, timezone, timedelta
import hashlib
import json
import os
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import sys
import time
import wave
import zipfile

REPORTS=['00-C6-RC1-Handoff.md','01-C6-A-R15-Defect-Closure.md','02-C6-A-Evidence-Closure.md',
         '03-C6-B-Widgets-QML-Parity.md','04-C6-C-Verification.md','05-C6-RC1-Defects-And-R16-Handoff.md']
def sha(data):return hashlib.sha256(data).hexdigest()
def digest(path):return sha(path.read_bytes())
def write_json(path,value):
    with path.open('x',encoding='utf-8') as file:json.dump(value,file,ensure_ascii=False,indent=2)
def archive(output,entries,prefix):
    assert not output.exists(),output
    with zipfile.ZipFile(output,'x') as result:
        for name,data in sorted(entries.items()):
            info=zipfile.ZipInfo(prefix+'/'+name,(1980,1,1,0,0,0));info.external_attr=0o100644<<16
            result.writestr(info,data,compress_type=zipfile.ZIP_DEFLATED,compresslevel=6)
    with zipfile.ZipFile(output) as result:assert result.testzip() is None

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','build','staging','publish','reports','evidence-root','evidence-list','full-summary','native-proof','isolation','verification'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--launcher-verification',type=Path,help='Required native matrix for Final-RC')
    parser.add_argument('--git',default=shutil.which('git'))
    parser.add_argument('--version',default='0.2.1-rc1')
    parser.add_argument('--display-version',default='V0.2.1-RC1')
    parser.add_argument('--status',default='V0.2.1-RC1 — DEVELOPER-VERIFIED — AWAITING HARNESS R16')
    parser.add_argument('--report-names',nargs='+',default=REPORTS)
    parser.add_argument('--complete-prefix',default='Relaxer-PhaseC-C6-RC1-Complete')
    parser.add_argument('--independent-acceptance',default='AWAITING HARNESS R16')
    args=parser.parse_args()
    source=args.source.resolve();build=args.build.resolve();stage=args.staging.resolve()
    git_env=os.environ.copy()
    git_env['PATH']=str(Path(args.git).resolve().parent)+os.pathsep+git_env.get('PATH','')
    git_env['MSYS']='noglob'
    def git(*command):return subprocess.check_output([args.git,*map(str,command)],cwd=source,env=git_env)
    assert not git('status','--porcelain').strip(),'Freeze clean source first'
    head=git('rev-parse','HEAD').decode().strip();tree=git('show','-s','--format=%T','HEAD').decode().strip()
    parent=git('show','-s','--format=%P','HEAD').decode().strip();branch=git('branch','--show-current').decode().strip()
    proof=json.loads(args.native_proof.read_text(encoding='utf-8'))
    deployed=json.loads((stage/'deployment.json').read_text(encoding='utf-8'))
    summary=json.loads(args.full_summary.read_text(encoding='utf-8'))
    fingerprint=(build/'relaxer-source-fingerprint.txt').read_text().strip()
    assert deployed['version'].lower()==args.version.lower() and deployed['displayVersion']==args.display_version
    assert proof['verified'] and deployed['verified']
    assert proof['sourceCommit']==deployed['sourceCommit']==summary['commit']==head
    assert proof['sourceTree']==deployed['sourceTree']==summary['tree']==tree
    assert proof['sourceFingerprint']==deployed['sourceFingerprint']==summary['sourceFingerprint']==fingerprint
    assert proof['binarySHA256']==deployed['binarySHA256']
    assert summary['ctest']['passed']>=75 and summary['ctest']['failed']==0 and summary['qtTotals']['failed']==0
    assert summary['baselineTestNamesRetained'] is True and summary['qtTotals']['blacklisted']==0
    native=json.loads(args.native_proof.with_name('result.json').read_text(encoding='utf-8'))
    assert len(native['suites'])==33 and native['qtTestTotals'][1]==0 and not native['launcherOwnedRemaining']
    assert native['paidRequests']==0 and all(row['exit']==0 for row in native['results'])
    launcher=json.loads(args.native_proof.with_name('launcher-result.json').read_text(encoding='utf-8'))
    assert launcher['launcherExit']==launcher['childExit']==launcher['childWait']==0 and not launcher['remainingOwned']
    isolation=json.loads((args.isolation/'results.json').read_text(encoding='utf-8'))
    assert len(isolation)==8
    for row in isolation:
        assert row['lockExit']==3 and row['qmlExit']==(11 if row['missing'] else 0) and not row['remainingOwned']
        assert row['widgets']['version'].lower()==args.version.lower() and row['dataIdentityPreserved']
        if row['missing']:assert row['restoredBothUis']
    unified=None
    if args.version == '0.2.1-final-rc':
        assert args.launcher_verification, 'Final-RC requires its own launcher matrix'
    if args.launcher_verification:
        unified=json.loads(args.launcher_verification.read_text(encoding='utf-8'))
        assert unified['status']=='PASS' and unified['caseCount']>=40
        assert unified['sourceCommit']==head and unified['sourceTree']==tree
        assert unified['sourceFingerprint']==fingerprint and unified['binarySHA256']==proof['binarySHA256']
        assert not unified['remainingOwned'] and all(not row['remainingOwned'] for row in unified['cases'])
        assert unified['delayedGuiLockExit']==3 and unified['friendlyMissingExit']==11
        assert unified['sharedDefaultDataRoot'] and unified['paidRequests']==0
    for name in args.report_names:assert Path(name).name==name and (args.reports/name).is_file(),name
    args.publish.mkdir(parents=True,exist_ok=False);args.verification.mkdir(parents=True,exist_ok=False)
    date=datetime.now(timezone(timedelta(hours=8))).strftime('%Y%m%d');prefix='Relaxer-'+args.display_version
    packager=runpy.run_path(str(source/'scripts/package-phase-c.py'))
    inventory=packager['inventory'](stage)
    win=args.publish/(prefix+'-Win64.zip')
    packager['package'](stage,win,prefix)
    with zipfile.ZipFile(win) as package_zip:
        (args.publish/'PACKAGE-MANIFEST.json').write_bytes(package_zip.read(prefix+'/PACKAGE-MANIFEST.json'))
    write_json(args.publish/'BUILD-IDENTITY.json',{'version':args.version,'displayVersion':args.display_version,
        'commit':head,'tree':tree,'parent':parent,'branch':branch,'sourceFingerprint':fingerprint,
        'binarySHA256':proof['binarySHA256'],'verified':True})

    # Git archive entries must match every frozen tree blob byte for byte.
    source_zip=args.publish/(prefix+'-Source.zip')
    subprocess.run([args.git,'archive','--format=zip','--prefix='+prefix+'-Source/','--output',str(source_zip),'HEAD'],cwd=source,env=git_env,check=True)
    rows=git('ls-tree','-r','-z','--full-tree','HEAD').split(b'\0');manifest={};blobs={}
    cat=subprocess.Popen([args.git,'cat-file','--batch'],cwd=source,env=git_env,stdin=subprocess.PIPE,stdout=subprocess.PIPE)
    try:
        with zipfile.ZipFile(source_zip) as archive_file:
            assert archive_file.testzip() is None
            for row in rows:
                if not row:continue
                attributes,path=row.split(b'\t',1);mode,kind,oid=attributes.split();assert kind==b'blob'
                name=path.decode('utf-8');cat.stdin.write(oid+b'\n');cat.stdin.flush()
                result=cat.stdout.readline().split();length=int(result[2]);blob=cat.stdout.read(length);assert cat.stdout.read(1)==b'\n'
                assert archive_file.read(prefix+'-Source/'+name)==blob,name
                manifest[name]={'gitBlob':oid.decode(),'sha256':sha(blob),'bytes':length,'mode':mode.decode()};blobs[name]=blob
            actual={n[len(prefix+'-Source/'): ] for n in archive_file.namelist() if not n.endswith('/')}
            assert actual==set(manifest),'Source ZIP entry set differs from frozen tree'
    finally:cat.stdin.close();cat.wait(timeout=20)
    selected=sorted(name for name in blobs if name=='CMakeLists.txt' or name.startswith('src/qml/') or name.startswith('resources/') or
        (name.startswith('src/') and name.endswith(('.cpp','.h'))) or (name.startswith('tests/') and name.endswith('.cpp')))
    identity='fingerprint-policy:'+sha(blobs['scripts/source-fingerprint.cmake'])+'\n'
    identity+=''.join(name+':'+sha(blobs[name])+'\n' for name in selected)
    assert sha(identity.encode('utf-8'))==fingerprint,'Archived source does not reproduce build fingerprint'
    write_json(args.publish/'SOURCE-TREE-MANIFEST.json',manifest)
    (args.publish/'Source-Fingerprint.txt').write_text(fingerprint+'\n',encoding='ascii',newline='\n')

    bundle=args.publish/(prefix+'-History.bundle')
    subprocess.run([args.git,'bundle','create',str(bundle),'--all'],cwd=source,env=git_env,check=True)
    result=subprocess.run([args.git,'bundle','verify',str(bundle)],cwd=source,env=git_env,capture_output=True,check=True)
    (args.verification/'bundle-verify.log').write_bytes(result.stdout+result.stderr)
    clone=args.verification/'bundle-clone'
    subprocess.run([args.git,'clone','--no-checkout',str(bundle),str(clone)],env=git_env,capture_output=True,check=True)
    assert subprocess.check_output([args.git,'rev-parse','HEAD'],cwd=clone,env=git_env).decode().strip()==head
    assert subprocess.check_output([args.git,'show','-s','--format=%T','HEAD'],cwd=clone,env=git_env).decode().strip()==tree
    fsck=subprocess.run([args.git,'fsck','--full'],cwd=clone,env=git_env,capture_output=True,check=True)
    (args.verification/'bundle-fsck.log').write_bytes(fsck.stdout+fsck.stderr)

    # Execute the final unzipped copy; keep it as separate disposable evidence.
    extracted=args.verification/'解压运行 中文 spaces';extracted.mkdir()
    with zipfile.ZipFile(win) as file:assert file.testzip() is None;file.extractall(extracted)
    runtime=extracted/prefix/'runtime'
    for name,expected in inventory.items():assert digest(extracted/prefix/name)==expected,name
    support=runpy.run_path(str(source/'scripts/measure-phase-c.py'))
    profile=args.verification/'profile';profile.mkdir();env=support['clean_environment'](profile);support['k'].SetErrorMode(0x8003)
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=5
    checked=[]
    def run(command,name):
        with (args.verification/(name+'.log')).open('xb') as output:
            process=subprocess.Popen(list(map(str,command)),cwd=args.verification,env=env,stdout=output,stderr=subprocess.STDOUT,startupinfo=startup)
            owned={process.pid};deadline=time.monotonic()+60
            while process.poll() is None:
                for pid,(parent_pid,_) in support['processes']().items():
                    if parent_pid in owned:owned.add(pid)
                if time.monotonic()>deadline:process.kill();process.wait();raise TimeoutError(name)
                time.sleep(.04)
            assert process.returncode==0,(name,process.returncode)
            assert not owned & support['processes']().keys(),name+' left a process'
            checked.append({'name':name,'exitCode':0,'ownedRemaining':[]})
    for mode in ('widgets','qml'):
        diagnostics=args.verification/(mode+'.json');data=args.verification/(mode+'-data')
        run([runtime/'Relaxer.exe','--ui='+mode,'--data',data,'--diagnostics',diagnostics,'--quit-after','500'],mode)
        value=json.loads(diagnostics.read_text(encoding='utf-8'))
        assert value['version'].lower()==args.version.lower() and value['saveSucceeded'] and value['databaseConnections']==1 and value['playbackInstances']==1
        assert not (data/'relaxer.lock').exists()
    if unified:
        for mode,flags in [('default',[]),('qml',['--qml']),('widgets',['--widgets'])]:
            diagnostics=args.verification/('launcher-'+mode+'.json');data=args.verification/('launcher-'+mode+'-data')
            run([runtime.parent/'RelaxerStart.exe',*flags,'--data',data,'--diagnostics',diagnostics,'--quit-after','350'],'launcher-'+mode)
            value=json.loads(diagnostics.read_text(encoding='utf-8'))
            assert value['mode']==('widgets' if mode=='widgets' else 'qml')
            assert value['version']==args.version and value['saveSucceeded']
            assert value['databaseConnections']==value['playbackInstances']==1
            assert not (data/'relaxer.lock').exists()
    audio=args.verification/'synthetic.wav'
    with wave.open(str(audio),'wb') as file:file.setparams((1,2,8000,0,'NONE','not compressed'));file.writeframes(b'\0'*16000)
    run([runtime/'Relaxer.exe','--media-smoke',audio],'pcm-decode')
    write_json(args.verification/'RESULT.json',{'status':'PASS','commit':head,'sourceFingerprint':fingerprint,'unzippedPrograms':checked,'sourceBlobsVerified':len(manifest),'bundleCloneVerified':True})

    # Compact only audit evidence; no executed runtime/profile/database payload.
    entries={};root=args.evidence_root.resolve()
    for selected_path in json.loads(args.evidence_list.read_text(encoding='utf-8')):
        path=Path(selected_path).resolve();assert path.is_relative_to(root),path
        files=[path] if path.is_file() else path.rglob('*')
        for file in files:
            if not file.is_file():continue
            relative=file.relative_to(root);parts=[p.lower() for p in relative.parts]
            if any(p in {'profile','temp','tmp','appdata','localappdata','.git','runtime','bundle-clone'} or p.endswith('-data') or p.startswith('portable ') for p in parts):continue
            if file.suffix.lower() not in {'.json','.txt','.log','.md','.png','.stdout','.stderr'}:continue
            if any(token in file.name.lower() for token in ('.sqlite','.db-wal','.db-shm','.key')):continue
            entries[relative.as_posix()]=file.read_bytes()
    assert entries
    entries['FinalExtractedVerification/RESULT.json']=(args.verification/'RESULT.json').read_bytes()
    for name in ('widgets.json','qml.json','widgets.log','qml.log','pcm-decode.log','bundle-verify.log','bundle-fsck.log'):
        entries['FinalExtractedVerification/'+name]=(args.verification/name).read_bytes()
    if unified:
        entries['UnifiedLauncher/RESULT.json']=args.launcher_verification.read_bytes()
        for path in args.verification.glob('launcher-*.*'):
            if path.is_file():entries['FinalExtractedVerification/'+path.name]=path.read_bytes()
    evidence_zip=args.publish/(prefix+'-Evidence.zip');archive(evidence_zip,entries,prefix+'-Evidence')
    report_copy=args.publish/'Reports';report_copy.mkdir()
    for name in args.report_names:shutil.copy2(args.reports/name,report_copy/name)
    artifacts={p.name:{'sha256':digest(p),'bytes':p.stat().st_size} for p in (win,source_zip,bundle,evidence_zip)}
    delivery={'status':args.status,'createdUtc':datetime.now(timezone.utc).isoformat(),
        'commit':head,'tree':tree,'parent':parent,'branch':branch,'sourceFingerprint':fingerprint,'binarySHA256':proof['binarySHA256'],
        'ctest':summary['ctest'],'qtTest':summary['qtTotals'],'fourServices':summary['fourServices'],'artifacts':artifacts,
        'portableQtTest':native['qtTestTotals'],'portableSuites':len(native['suites']),'isolationCases':len(isolation),
        'launcherClose':launcher,'baselineTestNamesRetained':summary['baselineTestNamesRetained'],
        'verification':str(args.verification/'RESULT.json'),'protocolVersion':'1.5.0 (retained startup protocol)',
        'paidRequests':0,'physicalListening':'NOT EXECUTED','physicalGlobalKeyboard':'NOT EXECUTED on isolated desktop; Windows registration/native message dispatch tested',
        'formalTagCreated':False,'remotePushed':False,'independentAcceptance':args.independent_acceptance}
    if unified:
        delivery['unifiedLauncher']={'status':unified['status'],'cases':unified['caseCount'],
            'delayedGuiLockExit':3,'friendlyMissingExit':11,'sharedDefaultDataRoot':True,
            'remainingOwned':[], 'extractedDefaultAndAliases':True}
    write_json(args.publish/'DELIVERY.json',delivery)
    files={p.relative_to(args.publish).as_posix():p.read_bytes() for p in args.publish.rglob('*') if p.is_file()}
    components=''.join(sha(data)+'  '+name+'\n' for name,data in sorted(files.items()))
    (args.publish/'SHA256SUMS-components.txt').write_text(components,encoding='utf-8',newline='\n');files['SHA256SUMS-components.txt']=components.encode()
    complete=args.publish/(args.complete_prefix+'-'+date+'.zip')
    archive(complete,files,args.complete_prefix+'-'+date)
    sums=''.join(digest(p)+'  '+p.relative_to(args.publish).as_posix()+'\n' for p in sorted(args.publish.rglob('*')) if p.is_file())
    (args.publish/'SHA256SUMS.txt').write_text(sums,encoding='utf-8',newline='\n')
    print(json.dumps({'status':delivery['status'],'publish':str(args.publish),'complete':str(complete),'completeSHA256':digest(complete),'commit':head,'tree':tree},ensure_ascii=False),flush=True)


if __name__=='__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    main()
