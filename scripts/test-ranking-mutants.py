"""Targeted temporary mutants using existing development build inputs.

Only fresh --output copies are written. Final source/objects/libraries stay
untouched. This is assertion discrimination, not a clean build claim.
"""
import argparse,ctypes as c,json,os,shutil,subprocess,sys
from ctypes import wintypes as w
from pathlib import Path
from evidence_run import execute

def split(command):
    shell=c.WinDLL('shell32');shell.CommandLineToArgvW.argtypes=[w.LPCWSTR,c.POINTER(c.c_int)];shell.CommandLineToArgvW.restype=c.POINTER(w.LPWSTR)
    kernel=c.WinDLL('kernel32');kernel.LocalFree.argtypes=[c.c_void_p]
    count=c.c_int();args=shell.CommandLineToArgvW(command,c.byref(count))
    try:return [args[i] for i in range(count.value)]
    finally:kernel.LocalFree(args)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('source','build','output'):p.add_argument('--'+key,type=Path,required=True)
    for key in ('ninja','ar'):p.add_argument('--'+key,required=True)
    args=p.parse_args();source,build,out=args.source.resolve(),args.build.resolve(),args.output.resolve();out.mkdir(parents=True,exist_ok=False);rows=[]
    cases=[('identity','lyrics-search-coordinator','staleRankingNeverPublishesBeforeFreshResult',
        'if (!live(j) || result.generation != j->generation || result.generation != m_generation ||\n        result.id != j->original.id || result.hash != j->original.hash)', 'if (false)'),
      ('epoch','lyrics-search-coordinator','staleRankingNeverPublishesBeforeFreshResult','result.epoch != epoch ||','false ||'),
      ('payload','lyrics-search-coordinator','staleRankingNeverPublishesBeforeFreshResult','result.payload != currentPayload ||','false ||'),
      ('fingerprint','lyrics-search-coordinator','staleRankingNeverPublishesBeforeFreshResult','result.fpO != fpO || result.fpM != fpM ||','false ||'),
      ('current','lyrics-search-coordinator','staleRankingNeverPublishesBeforeFreshResult','if (!current(j)) {','if (false) {'),
      ('calling-thread','import-coordinator','rejectsForeignCallingThreadAndLibraryAffinityBeforeWork','if(!ownerThread()){emit requestRejected','if(false){emit requestRejected')]
    for name,unit,test,old,new in cases:
        directory=out/name;directory.mkdir();file=source/'src'/f'{unit}.cpp';original=file.read_text(encoding='utf-8')
        if name=='current':
            prefix,body=original.split('void LyricsSearchCoordinator::applyRanking(',1);assert old in body
            mutated=prefix+'void LyricsSearchCoordinator::applyRanking('+body.replace(old,new,1)
        else:assert original.count(old)==1,(name,original.count(old));mutated=original.replace(old,new,1)
        cpp=directory/file.name;cpp.write_text(mutated,encoding='utf-8')
        target='relaxer_lyrics_search_coordinator_tests' if unit.startswith('lyrics') else 'relaxer_import_coordinator_tests'
        library='librelaxer_lyrics_search.a' if unit.startswith('lyrics') else 'librelaxer_core.a'
        commands=subprocess.check_output([args.ninja,'-C',str(build),'-t','commands',target+'.exe'],text=True).splitlines()
        compile_line=next(line for line in commands if line.endswith('/src/'+unit+'.cpp') and ' -c ' in line)
        compile_args=split(compile_line);obj=directory/(unit+'.cpp.obj')
        for flag,value in [('-o',str(obj)),('-MF',str(directory/'mutant.d')),('-MT',str(obj))]:compile_args[compile_args.index(flag)+1]=value
        compile_args[-1]=str(cpp)
        link=split(commands[-1].split('&&',1)[1].rsplit('&&',1)[0].strip())
        lib=directory/library;shutil.copy2(build/library,lib)
        link[link.index('-o')+1]=str(directory/'mutant.exe');link=[str(lib) if v==library else v for v in link]
        link=[('-Wl,--out-implib,'+str(directory/'mutant.dll.a')) if v.startswith('-Wl,--out-implib,') else v for v in link]
        for tag,command in [('compile',compile_args),('archive',[args.ar,'r',str(lib),str(obj)]),('link',link)]:
            r=execute(command,directory,tag,cwd=build,env=os.environ,identity={'mutant':name},timeout=120)
            if r['status']!='PASS':raise RuntimeError((name,tag,r))
        report=directory/'qt.txt';r=execute([directory/'mutant.exe',test,'-o',str(report)+',txt'],directory,'test',cwd=build,env=os.environ,identity={'mutant':name},timeout=45)
        text=report.read_text(encoding='utf-8',errors='replace');failures=[line for line in text.splitlines() if line.startswith('FAIL!')]
        row={'mutant':name,'source':str(cpp),'replacement':{'from':old,'to':new},'exitCode':r.get('exitCode'),'failures':failures,'caught':bool(failures) and r.get('exitCode',0)>0}
        rows.append(row);(out/'RESULT.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(row),flush=True)
        if not row['caught']:raise AssertionError('Mutant survived: '+name)
    return 0
if __name__=='__main__':sys.exit(main())
