# Windows PowerShell 5.1. CreateProcessW returns the owned handle atomically.
# No process is ever opened or terminated by PID, name, or ancestry here.
if (-not ('RelaxerVerification.NativeLease' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;
using System.ComponentModel;
using System.Runtime.InteropServices;
namespace RelaxerVerification {
 public interface ILease {
  bool Exited(); bool Wait(int milliseconds); void RequestClose(); void Terminate();
 }
 public static class Cleanup {
  public static string Run(ILease lease, int grace, int limit) {
   if (lease.Exited()) return "already-exited";
   lease.RequestClose();
   if (lease.Wait(grace)) return "closed";
   // A signaled retained handle, including exit between these calls, is safe.
   if (lease.Exited()) return "exited-before-termination";
   try { lease.Terminate(); }
   catch (Win32Exception) { if (!lease.Exited()) throw; }
   if (!lease.Wait(limit)) throw new TimeoutException("Owned handle did not signal");
   return "terminated-owned-handle";
  }
 }
 public sealed class NativeLease : ILease, IDisposable {
  [StructLayout(LayoutKind.Sequential)] struct SA { public int size; public IntPtr descriptor; public int inherit; }
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct SI {
   public int cb; public string reserved,desktop,title; public int x,y,xsize,ysize,xc,yc,fill,flags;
   public short show,reservedSize; public IntPtr reserved2,input,output,error;
  }
  [StructLayout(LayoutKind.Sequential)] struct PI { public IntPtr process,thread; public int pid,tid; }
  [DllImport("kernel32",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessW(string exe,StringBuilder command,IntPtr pa,IntPtr ta,bool inherit,int flags,IntPtr env,string cwd,ref SI si,out PI pi);
  [DllImport("kernel32",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateFileW(string name,uint access,uint share,ref SA sa,uint disposition,uint flags,IntPtr template);
  [DllImport("kernel32",SetLastError=true)] static extern bool GetProcessTimes(IntPtr h,out long created,out long exited,out long kernel,out long user);
  [DllImport("kernel32",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool QueryFullProcessImageNameW(IntPtr h,int flags,StringBuilder name,ref int length);
  [DllImport("kernel32",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr h,uint ms);
  [DllImport("kernel32",SetLastError=true)] static extern bool TerminateProcess(IntPtr h,uint code);
  [DllImport("kernel32",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr h,out int code);
  [DllImport("kernel32")] static extern bool CloseHandle(IntPtr h);
  delegate bool EnumWindow(IntPtr hwnd,IntPtr param);
  [DllImport("user32")] static extern bool EnumWindows(EnumWindow cb,IntPtr param);
  [DllImport("user32")] static extern int GetWindowThreadProcessId(IntPtr hwnd,out int pid);
  [DllImport("user32",SetLastError=true)] static extern bool PostMessageW(IntPtr hwnd,uint msg,IntPtr w,IntPtr l);
  IntPtr handle; public readonly int Pid,ParentPid; public readonly long Created;
  public readonly string Image,Command,Root,Task;
  public static bool Inside(string root,string image) {
   string boundary=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar)+Path.DirectorySeparatorChar;
   return Path.GetFullPath(image).StartsWith(boundary,StringComparison.OrdinalIgnoreCase);
  }
  public static string Quote(string value) {
   var b=new StringBuilder("\""); int slashes=0;
   foreach(char ch in value) {
    if(ch=='\\') { slashes++; continue; }
    if(ch=='"') b.Append('\\',slashes*2+1); else b.Append('\\',slashes);
    slashes=0; b.Append(ch);
   }
   b.Append('\\',slashes*2); return b.Append('"').ToString();
  }
  public NativeLease(string exe,string[] args,string cwd,string stdout,string stderr,string task) {
   Root=Path.GetFullPath(cwd); Task=task; exe=Path.GetFullPath(exe);
   // Verification executes only binaries from its fresh staging directory.
   if(!Inside(Root,exe)) throw new InvalidOperationException("Executable outside verification root");
   var command=new StringBuilder(Quote(exe)); foreach(string arg in args) command.Append(' ').Append(Quote(arg));
   Command=command.ToString(); ParentPid=System.Diagnostics.Process.GetCurrentProcess().Id;
   SA sa=new SA(); sa.size=Marshal.SizeOf(typeof(SA)); sa.inherit=1;
   IntPtr output=IntPtr.Zero,error=IntPtr.Zero,input=IntPtr.Zero;
   try {
    output=CreateFileW(stdout,0x40000000,1,ref sa,1,0,IntPtr.Zero); CheckFile(output);
    error=CreateFileW(stderr,0x40000000,1,ref sa,1,0,IntPtr.Zero); CheckFile(error);
    input=CreateFileW("NUL",0x80000000,3,ref sa,3,0,IntPtr.Zero); CheckFile(input);
    SI si=new SI(); si.cb=Marshal.SizeOf(typeof(SI)); si.flags=0x101; si.show=0;
    si.input=input; si.output=output; si.error=error; PI pi;
    if(!CreateProcessW(exe,new StringBuilder(Command),IntPtr.Zero,IntPtr.Zero,true,0x08000000,IntPtr.Zero,Root,ref si,out pi)) throw new Win32Exception();
    handle=pi.process; Pid=pi.pid; CloseHandle(pi.thread);
    long exit,kernel,user;
    if(!GetProcessTimes(handle,out Created,out exit,out kernel,out user)) throw new Win32Exception();
    var image=new StringBuilder(32768); int length=image.Capacity;
    if(!QueryFullProcessImageNameW(handle,0,image,ref length)) throw new Win32Exception();
    Image=Path.GetFullPath(image.ToString());
    if(!String.Equals(Image,exe,StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Created image mismatch");
   } catch {
    if(handle!=IntPtr.Zero) { Cleanup.Run(this,1000,5000); Dispose(); } throw;
   } finally {
    foreach(IntPtr h in new[]{output,error,input}) if(h!=IntPtr.Zero && h!=new IntPtr(-1)) CloseHandle(h);
   }
  }
  static void CheckFile(IntPtr h) { if(h==new IntPtr(-1)) throw new Win32Exception(); }
  public bool Exited() { return Wait(0); }
  public bool Wait(int milliseconds) {
   if(handle==IntPtr.Zero) throw new ObjectDisposedException("owned handle");
   uint result=WaitForSingleObject(handle,(uint)milliseconds);
   if(result==0xffffffff) throw new Win32Exception(); return result==0;
  }
  public int ExitCode { get { int code; if(!GetExitCodeProcess(handle,out code)) throw new Win32Exception(); return code; } }
  public void RequestClose() {
   if(Exited()) return;
   EnumWindows(delegate(IntPtr hwnd,IntPtr unused) {
    int pid; GetWindowThreadProcessId(hwnd,out pid);
    // The retained process handle prevents its PID from being reused.
    if(pid==Pid && !Exited()) PostMessageW(hwnd,0x10,IntPtr.Zero,IntPtr.Zero);
    return true;
   },IntPtr.Zero);
  }
  public void Terminate() { if(!TerminateProcess(handle,124)) throw new Win32Exception(); }
  public void Dispose() { if(handle!=IntPtr.Zero) { CloseHandle(handle); handle=IntPtr.Zero; } }
 }
}
'@
}
