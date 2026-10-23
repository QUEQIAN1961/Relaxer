"""Repeat unchanged assertions; interactive input/foreground blocks are explicit."""
import argparse,json,os,sys
from pathlib import Path
from evidence_run import execute

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--iterations',type=int,default=10);p.add_argument('--interactive',action='store_true')
    args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);rows=[];environment=dict(os.environ)
    environment['QT_QPA_PLATFORM']='windows' if args.interactive else 'offscreen'
    exe=args.build/('relaxer_translation_activation_tests.exe' if args.interactive else 'relaxer_translation_filter_performance_tests.exe')
    status='PASS'
    for index in range(1,args.iterations+1):
        report=out/f'{index:02d}.qt.txt';r=execute([exe,'-o',str(report)+',txt'],out,f'{index:02d}',env=environment,timeout=90)
        text=report.read_text(encoding='utf-8',errors='replace') if report.exists() else ''
        row={'iteration':index,'status':r['status'],'exitCode':r.get('exitCode'),'seconds':r['seconds'],'report':str(report)}
        if r['status']!='PASS':row['status']='BLOCKED' if 'BLOCKED:' in text else 'FAIL';status=row['status']
        rows.append(row);(out/'RESULT.json').write_text(json.dumps({'status':status,'requested':args.iterations,'executed':len(rows),'cases':rows},indent=2),encoding='utf-8')
        print(json.dumps(row),flush=True)
        if status!='PASS':break
    return 0 if status=='PASS' else 3 if status=='BLOCKED' else 1
if __name__=='__main__':sys.exit(main())
