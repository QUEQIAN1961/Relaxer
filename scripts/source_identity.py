"""Recompute the existing fingerprint policy from Git blobs, never mtimes."""
import hashlib,subprocess

def git(source,*args):return subprocess.check_output(['git',*args],cwd=source)
def sha(data):return hashlib.sha256(data).hexdigest()
def identity(source,revision='HEAD'):
    rows=git(source,'ls-tree','-r','-z',revision).split(b'\0');blobs={}
    for row in rows:
        if not row:continue
        attrs,path=row.split(b'\t');mode,kind,oid=attrs.split()
        if kind!=b'blob':raise ValueError('Unsupported non-blob tree entry')
        name=path.decode('utf-8')
        if name=='scripts/source-fingerprint.cmake' or name=='CMakeLists.txt' or name.startswith(('src/qml/','resources/')) or name.startswith('src/') and name.endswith(('.cpp','.h')) or name.startswith('tests/') and name.endswith('.cpp'):
            blobs[name]=git(source,'cat-file','blob',oid.decode())
    policy=blobs.pop('scripts/source-fingerprint.cmake')
    inputs='fingerprint-policy:'+sha(policy)+'\n'+''.join(name+':'+sha(data)+'\n' for name,data in sorted(blobs.items()))
    return {'commit':git(source,'rev-parse',revision).decode().strip(),'tree':git(source,'rev-parse',revision+'^{tree}').decode().strip(),'sourceFingerprint':sha(inputs.encode())}
