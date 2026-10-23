"""Read-only release gate. Never creates tags, pushes, or changes artifacts.

Local hashes/identity and a report's claimed issuer can be checked; a local JSON
file cannot prove human authorship or independent provenance. Review the named
Harness source/report out of band. Development self-tests are not acceptance.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
from source_identity import identity,git

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def safe_file(root,name):
    path=(root/name).resolve()
    if not path.is_relative_to(root.resolve()) or not path.is_file():raise ValueError('Missing/outside artifact: '+name)
    return path

def evaluate(source,delivery,attestation=None,*,fixture=False):
    result={'mode':'dry-run','eligible':False,'status':'BLOCKED','checks':{},
        'trustLimit':'Checks local identity/digests and issuer claims, not independent human authorship. Review provenance separately.'}
    try:
        if git(source,'status','--porcelain').strip():raise ValueError('Source worktree is not clean')
        actual=identity(source);declared=json.loads((delivery/'BUILD-IDENTITY.json').read_text(encoding='utf-8'))
        for key,value in actual.items():
            if declared.get(key)!=value:raise ValueError('Frozen identity mismatch: '+key)
        result['checks']['frozenIdentity']=actual
        expected={}
        for line in (delivery/'SHA256SUMS.txt').read_text(encoding='utf-8').splitlines():
            match=re.fullmatch(r'([0-9a-fA-F]{64})\s+\*?(.+)',line)
            if not match:raise ValueError('Malformed SHA256SUMS line')
            sha,name=match.groups()
            if name in expected:raise ValueError('Duplicate artifact hash')
            expected[name]=sha.lower()
            if digest(safe_file(delivery,name))!=sha.lower():raise ValueError('Artifact replaced: '+name)
        if not expected:raise ValueError('Empty artifact manifest')
        result['checks']['deliveryHashes']=len(expected)
        if attestation is None:
            result.update(status='PENDING',reason='Independent Harness attestation missing');return result
        claim=json.loads(attestation.read_text(encoding='utf-8'))
        if claim.get('schemaVersion')!=1:raise ValueError('Unsupported attestation schema')
        if claim.get('issuer',{}).get('kind')!='independent-harness':raise ValueError('Self-test or unknown issuer cannot approve a release')
        if claim.get('identity')!=actual:raise ValueError('Harness report belongs to another frozen identity')
        if claim.get('artifactHashes')!=expected:raise ValueError('Harness artifact hashes differ from delivery')
        report=safe_file(attestation.parent,claim['report']['path'])
        if digest(report)!=claim['report']['sha256']:raise ValueError('Harness report replaced')
        content=report.read_text(encoding='utf-8')
        if any(value not in content for value in actual.values()):raise ValueError('Harness report text is not bound to all identity fields')
        verdict=claim.get('verdict',{})
        for field in ('productRegression','artifactIntegrity'):
            if verdict.get(field)!='PASS':raise ValueError('Harness acceptance not satisfied: '+field)
        if verdict.get('processSafety') not in ('SAFE','SAFE WITH LIMITATIONS'):raise ValueError('Harness process safety not accepted')
        if verdict.get('freezeReadiness') not in ('READY','READY WITH CONDITIONS'):raise ValueError('Harness freeze readiness not accepted')
        conditions=claim.get('conditions')
        if not isinstance(conditions,list):raise ValueError('Explicit conditions array required, even when empty')
        result['checks']['independentReport']={'path':str(report),'sha256':digest(report),'claimedIssuer':claim['issuer']}
        result['checks']['conditions']=conditions
        if claim.get('testOnly'):
            result.update(status='TEST_ONLY' if fixture else 'BLOCKED',reason='Synthetic fixture is never release approval');return result
        if any(condition.get('status')!='SATISFIED' for condition in conditions):
            result.update(status='PENDING',reason='Independent verdict has unresolved release conditions');return result
        result.update(status='PASS',eligible=True)
    except Exception as error:result['reason']=str(error)
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True);p.add_argument('--delivery',type=Path,required=True);p.add_argument('--attestation',type=Path)
    args=p.parse_args();result=evaluate(args.source.resolve(),args.delivery.resolve(),args.attestation)
    print(json.dumps(result,ensure_ascii=False,indent=2));return 0 if result['eligible'] else 2
if __name__=='__main__':sys.exit(main())
