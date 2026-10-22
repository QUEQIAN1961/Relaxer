#!/usr/bin/env python3
"""Native, isolated unified-launcher verification; never runs an installed copy."""
import argparse, ctypes as c
from ctypes import wintypes as w
import hashlib, json, os
from pathlib import Path
import runpy, shutil, sqlite3, subprocess, time

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(); package=args.package.resolve(); out=args.output.resolve()
    out.mkdir(parents=True,exist_ok=False)
    support=runpy.run_path(str(Path(__file__).with_name('measure-phase-c.py')))
    env=support['clean_environment'](out); env.pop('QT_QPA_PLATFORM',None)
    k,u=support['k'],support['u']; k.SetErrorMode(0x8003)
    k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD]; k.OpenProcess.restype=w.HANDLE
    ps=c.WinDLL('psapi',use_last_error=True)
    ps.EnumProcessModules.argtypes=[w.HANDLE,c.POINTER(w.HMODULE),w.DWORD,c.POINTER(w.DWORD)]
    ps.GetModuleFileNameExW.argtypes=[w.HANDLE,w.HMODULE,w.LPWSTR,w.DWORD]
    u.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
    u.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
    u.EnumChildWindows.argtypes=[w.HWND,support['callback_type'],w.LPARAM]
    startup=subprocess.STARTUPINFO(); startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow=5
    rows=[]; active=[]
    def modules(pid):
        h=k.OpenProcess(0x410,False,pid); assert h,('OpenProcess',pid,c.get_last_error())
        try:
            array=(w.HMODULE*2048)(); size=w.DWORD()
            assert ps.EnumProcessModules(h,array,c.sizeof(array),c.byref(size))
            names=[]
            for module in array[:size.value//c.sizeof(w.HMODULE)]:
                buf=c.create_unicode_buffer(32768)
                assert ps.GetModuleFileNameExW(h,module,buf,len(buf))
                names.append(Path(buf.value).name)
            return sorted(names)
        finally:k.CloseHandle(h)
    def text(hwnd):
        buf=c.create_unicode_buffer(4096);u.GetWindowTextW(hwnd,buf,len(buf));return buf.value
    def dialogs(pids):
        found=[]
        for pid in pids:
            for hwnd in support['windows'](pid):
                buf=c.create_unicode_buffer(128);u.GetClassNameW(hwnd,buf,len(buf))
                if buf.value!='#32770' and text(hwnd)!='Relaxer':continue
                values=[text(hwnd)]
                @support['callback_type']
                def child(control,_):values.append(text(control));return True
                u.EnumChildWindows(hwnd,child,0)
                # QMessageBox is a Qt window, not a native #32770 dialog.
                # Its alien child widgets expose their text through UI Automation.
                if buf.value!='#32770':
                    powershell=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
                    command=('[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false); '
                        'Add-Type -AssemblyName UIAutomationClient; '
                        '$e=[System.Windows.Automation.AutomationElement]::FromHandle([IntPtr]'+str(hwnd)+'); '
                        '$e.FindAll([System.Windows.Automation.TreeScope]::Descendants,'
                        '[System.Windows.Automation.Condition]::TrueCondition) | '
                        'ForEach-Object { $_.Current.Name }')
                    result=subprocess.run([str(powershell),'-NoLogo','-NoProfile','-NonInteractive','-Command',command],
                        capture_output=True,timeout=10,creationflags=subprocess.CREATE_NO_WINDOW)
                    assert result.returncode==0,result.stderr
                    values.append(result.stdout.decode('utf-8',errors='replace'))
                found.append((hwnd,'\n'.join(values)))
        return found
    def track(item):
        for pid,(parent,name) in support['processes']().items():
            if parent in item['owned']:item['owned'].add(pid)
    def start(command,tag):
        f=(out/(tag+'.log')).open('xb')
        p=subprocess.Popen(command if isinstance(command,str) else list(map(str,command)),cwd=out,env=env,
            stdout=f,stderr=subprocess.STDOUT,startupinfo=startup)
        item={'p':p,'owned':{p.pid},'tag':tag,'file':f};active.append(item);return item
    def finish(item,expected=0):
        deadline=time.monotonic()+30
        while item['p'].poll() is None and time.monotonic()<deadline:track(item);time.sleep(.025)
        assert item['p'].poll()==expected,(item['tag'],item['p'].poll(),expected)
        item['file'].close();track(item)
        remaining=sorted(item['owned'] & support['processes']().keys());assert not remaining,(item['tag'],remaining)
        rows.append({'case':item['tag'],'exit':expected,'remainingOwned':remaining})
        (out/'rows.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
        return rows[-1]
    def ready(item,mode,report=None):
        deadline=time.monotonic()+30
        while time.monotonic()<deadline:
            track(item);assert item['p'].poll() is None,(item['tag'],item['p'].returncode)
            table=support['processes']()
            players=[pid for pid in item['owned'] if pid in table and table[pid][1].lower()=='relaxer.exe']
            if len(players)==1:
                pid=players[0];hwnds=support['windows'](pid)
                if hwnds and (report is None or report.exists()):
                    loaded=modules(pid)
                    qml='RelaxerQmlUi.dll' in loaded
                    assert qml==(mode=='qml'),(item['tag'],loaded)
                    assert ('Qt6Quick.dll' in loaded)==qml
                    assert len(hwnds)==1,(item['tag'],hwnds)
                    assert all(table[p][1].lower()!='conhost.exe' for p in item['owned'] if p in table)
                    if report:
                        value=json.loads(report.read_text(encoding='utf-8'));assert value['mode']==mode
                        # Widgets emits a database-ready snapshot before the
                        # window-ready snapshot; retain the full invariant and
                        # wait for its fields instead of treating existence as readiness.
                        if 'databaseConnections' not in value or 'playbackInstances' not in value:
                            time.sleep(.025)
                            continue
                        assert value['databaseConnections']==value['playbackInstances']==1
                    item.update(player=pid,hwnds=hwnds,loaded=loaded);return
            time.sleep(.04)
        raise AssertionError('No ready main window: '+item['tag'])
    def close(item,report=None):
        for hwnd in item['hwnds']:assert u.PostMessageW(hwnd,0x10,0,0)
        row=finish(item)
        row['runtimePid']=item['player'];row['modules']=item['loaded']
        if report:
            value=json.loads(report.read_text(encoding='utf-8'));assert value['saveSucceeded'] and value['exitCode']==0
            assert not (Path(value['dataRoot'])/'relaxer.lock').exists();row['diagnostics']=value
        return row
    def cmdline(copy,extra=''):
        cmd=Path(os.environ['SystemRoot'])/'System32/cmd.exe'
        return f'{cmd} /d /s /c ""{copy / "RelaxerQml.cmd"}" {extra}"'
    def clone(name):
        dest=out/name
        shutil.copytree(package,dest,ignore=shutil.ignore_patterns('data','startup.log','startup.lock'))
        assert not (dest/'data').exists();return dest
    try:
        copy=clone('全新 解压 中文 path with spaces');launcher=copy/'RelaxerStart.exe';runtime=copy/'runtime/Relaxer.exe'
        # The true no-argument entry must share the existing default data root
        # with both aliases, direct runtime and the historical CMD.
        entries=[('default', [launcher], 'qml'),('explicit-qml',[launcher,'--qml'],'qml'),
                 ('widgets',[launcher,'--widgets'],'widgets'),('runtime',[runtime,'--ui=qml'],'qml'),('cmd',cmdline(copy),'qml')]
        for tag,command,mode in entries:
            item=start(command,'entry-'+tag);ready(item,mode);close(item)
            db=copy/'data/relaxer.sqlite';assert db.is_file() and not (copy/'data/relaxer.lock').exists()
            with sqlite3.connect(db) as connection:
                if tag=='default':connection.execute("INSERT INTO settings(key,value) VALUES('launcher.sentinel','[123]')")
                assert connection.execute("SELECT value FROM settings WHERE key='launcher.sentinel'").fetchone()==('[123]',)
            assert list(copy.rglob('*.sqlite'))==[db]
            assert not (copy/'startup.lock').exists()
        # Forward paths relative to the caller, including initially absent roots.
        for selector in ([],['--qml'],['--widgets'],['--ui','widgets']):
            tag='forward-'+('-'.join(selector) or 'default').replace('--','')
            data=Path('不存在 中文 data '+tag);report=out/(tag+'.json')
            item=start([launcher,*selector,'--data',data,'--diagnostics',report,'--quit-after','350'],tag)
            finish(item)
            value=json.loads(report.read_text(encoding='utf-8'))
            assert Path(value['dataRoot']).resolve()==(out/data).resolve() and value['saveSucceeded']
            assert value['databaseConnections']==value['playbackInstances']==1
            assert not (out/data/'relaxer.lock').exists()
        # All four cross-UI locks and subsequent relaunch, using the same launcher.
        for first,second in [('qml','qml'),('qml','widgets'),('widgets','qml'),('widgets','widgets')]:
            tag=first+'-'+second;data=out/('lock-'+tag);report=out/(tag+'-first.json')
            item=start([launcher,*([] if first=='qml' else ['--widgets']),'--data',data,'--diagnostics',report],tag+'-first')
            ready(item,first,report)
            other=start([launcher,*([] if second=='qml' else ['--widgets']),'--data',data,'--diagnostics',out/(tag+'-second.json')],tag+'-second')
            finish(other,3);assert item['p'].poll() is None and support['windows'](item['player'])
            close(item,report)
            again=start([launcher,'--data',data,'--diagnostics',out/(tag+'-relaunch.json'),'--quit-after','200'],tag+'-relaunch');finish(again)
        # Retain the ordinary runtime dialog and wait > historical detach window.
        report=out/'dialog-first.json';data=out/'dialog-data'
        item=start([launcher,'--data',data,'--diagnostics',report],'dialog-first');ready(item,'qml',report)
        other=start([launcher,'--widgets','--data',data],'delayed-lock-dialog')
        deadline=time.monotonic()+20;found=[]
        while time.monotonic()<deadline:
            track(other);found=dialogs(other['owned'])
            if found:break
            time.sleep(.05)
        assert found
        assert any('另一个 Relaxer' in value for _,value in found),found
        (out/'lock-dialog.txt').write_text('\n'.join(v for _,v in found),encoding='utf-8')
        time.sleep(3.1);assert other['p'].poll() is None
        for hwnd,_ in found:assert u.PostMessageW(hwnd,0x10,0,0)
        finish(other,3);assert item['p'].poll() is None;close(item,report)
        # Each missing optional component: controlled launcher/runtime exit 11,
        # no dataRoot, independent Widgets, then restored QML.
        for filename in ['RelaxerQmlUi.dll','Qt6Qml.dll','Qt6Quick.dll','Qt6QuickControls2.dll']:
            missing=clone('missing-'+filename);target=missing/'runtime'/filename;target.unlink()
            for selector in ([],['--qml']):
                tag='missing-'+filename+('-explicit' if selector else '-default');data=out/(tag+'-absent')
                p=start([missing/'RelaxerStart.exe',*selector,'--data',data,'--diagnostics',out/(tag+'.json')],tag)
                finish(p,11);assert not data.exists() and not (missing/'data').exists()
                assert 'Optional QML module' in (missing/'runtime/startup.log').read_text(encoding='utf-8',errors='replace')
            p=start([missing/'runtime/Relaxer.exe','--ui=qml','--data',data],'runtime-missing-'+filename);finish(p,11);assert not data.exists()
            report=out/('missing-'+filename+'-widgets.json')
            p=start([missing/'RelaxerStart.exe','--widgets','--data',out/('widgets-'+filename),'--diagnostics',report],filename+'-widgets')
            ready(p,'widgets',report);close(p,report)
            if filename=='RelaxerQmlUi.dll':
                p=start([missing/'RelaxerStart.exe','--data',data],'friendly-missing-dialog');deadline=time.monotonic()+20;found=[]
                while time.monotonic()<deadline:
                    track(p);found=dialogs(p['owned'])
                    if found:break
                    time.sleep(.05)
                assert any('新版界面的组件缺失' in v and '--widgets' in v and '11' in v for _,v in found),found
                (out/'missing-dialog.txt').write_text('\n'.join(v for _,v in found),encoding='utf-8')
                for hwnd,_ in found:assert u.PostMessageW(hwnd,0x10,0,0)
                finish(p,11);assert not data.exists()
            shutil.copy2(package/'runtime'/filename,target)
            p=start([missing/'RelaxerStart.exe','--data',out/('restored-'+filename),'--diagnostics',out/(filename+'-restored.json'),'--quit-after','250'],'restored-'+filename);finish(p)
        # CLI failure remains a failure, no accidental library creation.
        for flags,tag in [(['--qml','--widgets'],'conflict'),(['--ui=invalid'],'bad-ui'),(['--page=invalid'],'bad-page')]:
            data=out/(tag+'-absent');p=start([launcher,*flags,'--data',data,'--diagnostics',out/(tag+'.json')],tag);finish(p,64);assert not data.exists()
        # Inspect real PE imports independently of the loaded module snapshots.
        objdump=shutil.which('objdump')
        assert objdump,'objdump required for PE dependency audit'
        imports={}
        for name,path in [('launcher',launcher),('runtime',runtime),('qml',copy/'runtime/RelaxerQmlUi.dll')]:
            # MinGW objdump's filename argument is ANSI; let CreateProcessW set
            # the Unicode working directory and pass only the ASCII basename.
            result=subprocess.run([objdump,'-p',path.name],cwd=path.parent,capture_output=True,check=True)
            raw=result.stdout.decode('utf-8',errors='replace');(out/(name+'-pe.txt')).write_text(raw,encoding='utf-8')
            imports[name]=[line.split('DLL Name:')[1].strip() for line in raw.splitlines() if 'DLL Name:' in line]
        assert not any('qt6' in s.lower() for s in imports['launcher'])
        assert not any('qt6quick' in s.lower() or 'qt6qml' in s.lower() for s in imports['runtime'])
        assert any('Qt6Quick' in s for s in imports['qml']) and any('Qt6Qml' in s for s in imports['qml'])
        deployment=json.loads((package/'deployment.json').read_text(encoding='utf-8'))
        value={'status':'PASS','cases':rows,'caseCount':len(rows),'remainingOwned':[],
            'sourceCommit':deployment['sourceCommit'],'sourceTree':deployment['sourceTree'],
            'sourceFingerprint':deployment['sourceFingerprint'],'binarySHA256':deployment['binarySHA256'],
            'peImports':imports,'sharedDefaultDataRoot':True,'delayedGuiLockExit':3,'friendlyMissingExit':11,
            'noPersonalDataUsed':True,'paidRequests':0,'physicalInput':False}
        (out/'RESULT.json').write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps({'status':'PASS','cases':len(rows)},ensure_ascii=False),flush=True)
    finally:
        # Only owned disposable test processes are cleaned on assertion failure.
        for item in active:
            track(item)
            for pid in item['owned'] & support['processes']().keys():
                handle=k.OpenProcess(1,False,pid)
                if handle:
                    k.TerminateProcess.argtypes=[w.HANDLE,w.UINT];k.TerminateProcess(handle,124);k.CloseHandle(handle)
            item['file'].close()

if __name__=='__main__': main()
