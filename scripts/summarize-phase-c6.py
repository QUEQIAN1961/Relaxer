#!/usr/bin/env python3
"""Read immutable C6 runner evidence and preserve its actual test counts."""
import argparse
import json
from pathlib import Path
import re
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('run','build','output'):
        parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--expect-tests',type=int,default=74)
    parser.add_argument('--baseline-tests-file',type=Path)
    args=parser.parse_args()
    results=json.loads((args.run/'results.json').read_text(encoding='utf-8'))
    assert results and all(r['exitCode']==0 for r in results),results
    assert len({r['commit'] for r in results})==1
    ctest=args.run/'ctest'
    text=(ctest/'output.log').read_text(encoding='utf-8',errors='replace')
    match=re.search(r'100% tests passed(?:, 0 tests failed)? out of (\d+)',text)
    assert match and int(match[1])>=args.expect_tests,text[-1000:]
    suites={};skips=[];totals=[0,0,0,0]
    for path in (ctest/'Testing/QtResults').glob('*.txt'):
        raw=path.read_bytes()
        try:content=raw.decode('utf-8')
        except UnicodeDecodeError:content=raw.decode('gb18030',errors='replace')
        matches=re.findall(r'Totals: (\d+) passed, (\d+) failed, (\d+) skipped, (\d+) blacklisted',content)
        assert len(matches)==1,path
        values=list(map(int,matches[0]));assert values[1]==0 and values[3]==0,path
        suites[path.stem]=values;totals=[a+b for a,b in zip(totals,values)]
        skips.extend({'suite':path.stem,'line':line} for line in content.splitlines() if line.startswith('SKIP '))
    services={k:v for k,v in suites.items() if k in ('relaxer_playback_facade','relaxer_lyrics_search_coordinator','relaxer_translation_coordinator','relaxer_import_coordinator')}
    if args.expect_tests>=74:assert len(services)==4,services
    def names(path):return set(re.findall(r'add_test\("([^"]+)"',path.read_text(encoding='utf-8')))
    retained=None
    if args.baseline_tests_file:
        retained=names(args.baseline_tests_file)<=names(args.build/'CTestTestfile.cmake')
        assert retained,'A baseline CTest name was removed'
    summary={'commit':results[0]['commit'],'tree':results[0]['tree'],
        'sourceFingerprint':(args.build/'relaxer-source-fingerprint.txt').read_text().strip(),
        'build':str(args.build.resolve()),'run':str(args.run.resolve()),'ctest':{'passed':int(match[1]),'failed':0},
        'qtTotals':dict(zip(('passed','failed','skipped','blacklisted'),totals)),
        'qtSuites':len(suites),'suites':suites,'skips':skips,'fourServices':services,'baselineTestNamesRetained':retained}
    with args.output.open('x',encoding='utf-8') as file:json.dump(summary,file,ensure_ascii=False,indent=2)
    print(json.dumps({key:value for key,value in summary.items() if key not in ('suites','skips')},ensure_ascii=False))


if __name__=='__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    main()
