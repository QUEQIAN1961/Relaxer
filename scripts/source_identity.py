"""Recompute the existing fingerprint policy from Git blobs, never mtimes."""
import hashlib,subprocess

def git(source,*args,executable='git'):return subprocess.check_output([executable,*args],cwd=source)
def sha(data):return hashlib.sha256(data).hexdigest()
def identity(source,revision='HEAD',git_executable='git'):
    def query(*args):return git(source,*args,executable=git_executable)
    rows=query('ls-tree','-r','-z',revision).split(b'\0');blobs={}
    for row in rows:
        if not row:continue
        attrs,path=row.split(b'\t');mode,kind,oid=attrs.split()
        if kind!=b'blob':raise ValueError('Unsupported non-blob tree entry')
        name=path.decode('utf-8')
        if name=='scripts/source-fingerprint.cmake' or name=='CMakeLists.txt' or name.startswith(('src/qml/','resources/')) or name.startswith('src/') and name.endswith(('.cpp','.h')) or name.startswith('tests/') and name.endswith(('.cpp','.h')):
            blobs[name]=query('cat-file','blob',oid.decode())
    policy=blobs.pop('scripts/source-fingerprint.cmake')
    inputs='fingerprint-policy:'+sha(policy)+'\n'+''.join(name+':'+sha(data)+'\n' for name,data in sorted(blobs.items()))
    return {'commit':query('rev-parse',revision).decode().strip(),'tree':query('show','-s','--format=%T',revision).decode().strip(),'sourceFingerprint':sha(inputs.encode())}
