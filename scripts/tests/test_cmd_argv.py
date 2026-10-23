"""Run with each supported Python distribution; no public/network requests."""
import argparse,json,os,platform,subprocess,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from launcher_observation import cmd_invocation,cmd_environment

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--echo-python',required=True);args=parser.parse_args()
    root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    directory=root/'中文 空格 (括号) & Unicode 音樂';directory.mkdir()
    echo=directory/'echo.py';echo.write_text('import json,sys\nprint(json.dumps(sys.argv[1:],ensure_ascii=True))\n',encoding='utf-8')
    script=directory/'argv echo & (compat).cmd'
    script.write_bytes(b'@echo off\r\nsetlocal DisableDelayedExpansion\r\n"%RELAXER_ARGV_ECHO_PYTHON%" "%~dp0echo.py" %*\r\nexit /b %errorlevel%\r\n')
    sent=['中文','a b','(parentheses)','literal & echo INJECTED','Unicode 音樂','','C:'+chr(92)+'trailing'+chr(92),'bang!caret^','semi;colon']
    environment=cmd_environment(os.environ);environment['RELAXER_ARGV_ECHO_PYTHON']=args.echo_python
    command=cmd_invocation(script,sent)
    result=subprocess.run(command,env=environment,capture_output=True,timeout=15)
    record={'sysExecutable':sys.executable,'platform':sys.platform,'osName':os.name,'pythonVersion':platform.python_version(),
            'command':command,'sent':sent,'exitCode':result.returncode,'stdout':result.stdout.decode(errors='replace'),'stderr':result.stderr.decode(errors='replace')}
    try:
        record['argv']=json.loads(result.stdout);record['status']='PASS' if result.returncode==0 and record['argv']==sent else 'FAIL'
    except Exception as error:record.update(status='FAIL',error=repr(error))
    (root/'RESULT.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(record,ensure_ascii=True));return 0 if record['status']=='PASS' else 1
if __name__=='__main__':sys.exit(main())
