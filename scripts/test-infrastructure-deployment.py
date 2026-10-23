"""Isolated CMake/Qt startup diagnostics; never alters installed dependencies."""
import argparse,ctypes as c,json,os,subprocess,sys
from pathlib import Path
from evidence_run import execute

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('source','build','qt','quick','output'):p.add_argument('--'+key,type=Path,required=True)
    args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);env=dict(os.environ,QT_LOGGING_TO_CONSOLE='1',QT_ASSUME_STDERR_HAS_CONSOLE='1');rows=[]
    c.WinDLL('kernel32').SetErrorMode(0x8003)
    cmake=args.qt/'bin/cmake.exe'
    configs=[('production-off-no-sqlite',['-DBUILD_TESTING=OFF','-DCMAKE_DISABLE_FIND_PACKAGE_SQLite3=TRUE'],0),
             ('testing-on-no-sqlite',['-DBUILD_TESTING=ON','-DCMAKE_DISABLE_FIND_PACKAGE_SQLite3=TRUE'],1),
             ('import-only',['-DBUILD_TESTING=ON','-DRELAXER_IMPORT_ONLY=ON','-DCMAKE_DISABLE_FIND_PACKAGE_Qt6Widgets=TRUE'],0),
             ('queue-only-no-sqlite',['-DRELAXER_QUEUE_ONLY=ON','-DCMAKE_DISABLE_FIND_PACKAGE_SQLite3=TRUE'],0)]
    for name,flags,expected in configs:
        directory=out/name
        cmd=[cmake,'-S',args.source,'-B',directory,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_PREFIX_PATH='+args.qt.as_posix(),'-DQT_ADDITIONAL_PACKAGES_PREFIX_PATH='+args.quick.as_posix(),*flags]
        r=execute(cmd,out,name,env=env,expected=expected,timeout=120);rows.append({'case':name,'status':r['status'],'exitCode':r.get('exitCode')})
        if r['status']!='PASS':raise AssertionError(rows[-1])
        if name=='production-off-no-sqlite':
            targets=subprocess.check_output([args.qt/'bin/ninja.exe','-C',directory,'-t','targets','all'],env=env,text=True)
            assert 'relaxer_import_coordinator_tests' not in targets and 'relaxer_translation_filter_performance_tests' not in targets
            (out/'production-targets.txt').write_text(targets,encoding='utf-8')
    binaries=['relaxer_import_widgets_tests.exe','relaxer_translation_filter_performance_tests.exe']
    for binary in binaries:
        for platform in ('default','windows','offscreen'):
            case=binary+'-'+platform;environment=dict(env);environment.pop('QT_QPA_PLATFORM',None)
            command=[args.build/binary,'-functions']
            if platform!='default':command+=['-platform',platform]
            r=execute(command,out,case,env=environment,timeout=15)
            raw=Path(r['streams']['stderr']['path']).read_text(errors='replace')
            assert r['status']=='PASS' and 'GUI-TEST-START' in raw,(case,r)
            rows.append({'case':case,'status':'PASS','exitCode':r['exitCode']})
        for platform in ('windows','offscreen'):
            for defect in ('missing','corrupt'):
                case=binary+'-'+platform+'-'+defect;root=out/case;root.mkdir();(root/'platforms').mkdir()
                if defect=='corrupt':(root/'platforms'/('q'+platform+'.dll')).write_bytes(b'controlled invalid DLL')
                environment=dict(env,RELAXER_TEST_PLUGIN_ROOT=str(root),QT_PLUGIN_PATH=str(root),QT_QPA_PLATFORM=platform)
                r=execute([args.build/binary,'-functions'],out,case,env=environment,timeout=15,expected=78)
                raw=Path(r['streams']['stderr']['path']).read_text(errors='replace')
                assert r['status']=='PASS' and r.get('exitCode')==78 and 'GUI-TEST-START' in raw and 'Qt platform plugin could not load' in raw,(case,r,raw)
                rows.append({'case':case,'status':'PASS (expected startup failure)','exitCode':r['exitCode']})
    (out/'RESULT.json').write_text(json.dumps({'status':'PASS','cases':rows},indent=2),encoding='utf-8')
if __name__=='__main__':main()
