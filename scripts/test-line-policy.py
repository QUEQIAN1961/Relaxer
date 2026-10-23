"""Two fresh Git checkouts plus local-only PowerShell/MSYS parser/reuse checks."""
import argparse,hashlib,json,os,subprocess,sys,zipfile
from pathlib import Path
from evidence_run import execute

def counts(data):
    pairs=data.count(b'\r\n');return {'CRLF':pairs,'bareLF':data.count(b'\n')-pairs,'bareCR':data.count(b'\r')-pairs,'BOM':data.startswith(b'\xef\xbb\xbf'),'sha256':hashlib.sha256(data).hexdigest()}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--bash',required=True)
    args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);env=os.environ.copy();rows=[]
    rel='scripts/online-msys2.ps1';blob=subprocess.check_output(['git','show','HEAD:'+rel],cwd=args.source)
    baseline=subprocess.check_output(['git','show','9180d872:'+rel],cwd=args.source)
    assert blob==baseline,'Line policy must not smuggle script logic edits'
    powershell=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
    for mode in ('true','false'):
        clone=out/('checkout-'+mode)
        for name,cmd in [('clone',['git','clone','--no-hardlinks','--no-checkout',args.source,clone]),('checkout',['git','-C',clone,'-c','core.autocrlf='+mode,'checkout','--detach','HEAD'])]:
            r=execute(cmd,out,mode+'-'+name,env=env,timeout=60);assert r['status']=='PASS',r
        data=(clone/rel).read_bytes();assert data==blob
        archive=out/(mode+'.zip');subprocess.check_call(['git','-C',clone,'archive','--format=zip','--output',archive,'HEAD'])
        with zipfile.ZipFile(archive) as z:assert z.read(rel)==blob
        # Pin the reuse marker and a dummy bash path; this returns before every
        # network/extraction/child-shell path. No online HTTP request is made.
        cache=clone/'fixture-cache';(cache/'msys64/usr/bin').mkdir(parents=True)
        (cache/'msys64/usr/bin/bash.exe').write_bytes(b'never executed')
        manifest=json.loads((clone/'scripts/msys2-download.json').read_text())
        (cache/'.relaxer-owned-cache').write_text('Relaxer-owned-cache-v1\n'+str(clone)+'\n',encoding='utf-8',newline='\n')
        (cache/'msys64/.relaxer-initialized').write_text(manifest['sha256'],encoding='ascii')
        child=dict(env,RELAXER_SOURCE_ROOT=str(clone),RELAXER_BUILD_CACHE=str(cache),MSYS2_ARG_CONV_EXCL='*')
        script=clone/rel
        # -File executes the exact LF script through both native and MSYS Bash
        # entry paths, with only the local reuse branch reachable.
        native=[powershell,'-NoLogo','-NoProfile','-NonInteractive','-File',script]
        invocation=out/(mode+'-invoke.sh');invocation.write_text('#!/bin/bash\nexec "$1" -NoLogo -NoProfile -NonInteractive -File "$2"\n',encoding='ascii',newline='\n')
        # A real script avoids the Windows-to-MSYS nested -c quoting boundary.
        bash=[args.bash,'--noprofile','--norc',invocation.as_posix(),powershell.as_posix(),script.as_posix()]
        for tag,cmd in [('native',native),('msys',bash)]:
            r=execute(cmd,out,mode+'-'+tag,env=child,timeout=30);assert r['status']=='PASS',r
            assert 'reuse-environment' in Path(r['streams']['stdout']['path']).read_text(errors='replace')
        rows.append({'autocrlf':mode,'blob':counts(blob),'worktree':counts(data),'archiveIdentical':True,'localReuseNativeAndMsys':True})
    (out/'RESULT.json').write_text(json.dumps({'status':'PASS','before':counts(baseline),'after':counts(blob),'cases':rows,'networkRequests':0},indent=2),encoding='utf-8')
if __name__=='__main__':main()
