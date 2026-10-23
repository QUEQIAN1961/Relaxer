param([Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
$implementation=Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\owned-process.ps1') -Raw
$leaseSource=($implementation -split "@'`r?`n",2)[1] -split "`r?`n'@",2
$root=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $root){throw 'Fresh output required'}
$null=New-Item -ItemType Directory -Path $root
$rows=New-Object Collections.Generic.List[object]
Add-Type -TypeDefinition ($leaseSource[0]+@'
public sealed class FakeLease : RelaxerVerification.ILease {
 public bool done,deny,race,graceful; public int close,kill,wait,poll;
 public int pid,ppid;
 public bool Exited(){poll++; return done;}
 public bool Wait(int ms){wait++; if(kill>0 && !deny)done=true; return done;}
 public void RequestClose(){close++;if(graceful)done=true;}
 public void Terminate(){kill++; if(race){done=true;throw new System.ComponentModel.Win32Exception(5);} if(deny)throw new System.ComponentModel.Win32Exception(5); done=true;}
}
'@)
function Check([string]$Name,[scriptblock]$Action){ & $Action; $rows.Add([ordered]@{case=$Name;status='PASS'}) }
try {
 Check 'already exited' { $p=New-Object FakeLease; $p.done=$true; if([RelaxerVerification.Cleanup]::Run($p,0,0)-ne 'already-exited' -or $p.kill){throw 'Exited process targeted'} }
 Check 'normal close before termination' { $p=New-Object FakeLease;$p.graceful=$true;if([RelaxerVerification.Cleanup]::Run($p,0,0)-ne 'closed' -or $p.kill){throw 'Graceful close not honored'} }
 Check 'timeout and repeated cleanup' { $p=New-Object FakeLease; $null=[RelaxerVerification.Cleanup]::Run($p,0,0); $null=[RelaxerVerification.Cleanup]::Run($p,0,0); if($p.kill-ne 1 -or $p.close-ne 1){throw 'Cleanup not idempotent'} }
 Check 'exit at termination boundary' { $p=New-Object FakeLease; $p.race=$true; $null=[RelaxerVerification.Cleanup]::Run($p,0,0); if(-not $p.done){throw 'Race not observed'} }
 Check 'access denied must fail' { $p=New-Object FakeLease; $p.deny=$true; $rejected=$false; try{$null=[RelaxerVerification.Cleanup]::Run($p,0,0)}catch{$rejected=$true}; if(-not $rejected -or $p.done){throw 'Denied cleanup became success'} }
 Check 'canonical directory prefix collision' {if([RelaxerVerification.NativeLease]::Inside($root,($root+'-other\same.exe'))){throw 'Prefix admitted'} }
 Check 'stale PID and PPID aliases stay unowned' { $owned=New-Object FakeLease;$owned.pid=42;$foreign=New-Object FakeLease;$foreign.pid=42;$foreign.ppid=42;$null=[RelaxerVerification.Cleanup]::Run($owned,0,0);if($foreign.kill -or $foreign.close -or $foreign.wait -or $foreign.poll){throw 'Unowned alias was consulted'} }
 # Build only a benign, owned fixture. No real process impersonation or PID
 # reuse is attempted. Raw handle ownership has no PID/PPID admission API.
 $fixture=Join-Path $root 'same.exe'
 Add-Type -TypeDefinition 'public class Fixture { public static int Main(string[] args) { if(args.Length>0) System.Threading.Thread.Sleep(int.Parse(args[0])); return 0; } }' -OutputAssembly $fixture -OutputType ConsoleApplication
 $canary=New-Object RelaxerVerification.NativeLease($fixture,@('20000'),$root,(Join-Path $root 'canary.out'),(Join-Path $root 'canary.err'),'controlled-canary')
 try {
  Check 'normal owned exit' {$p=New-Object RelaxerVerification.NativeLease($fixture,@('0'),$root,(Join-Path $root 'normal.out'),(Join-Path $root 'normal.err'),'normal');try{if(-not $p.Wait(5000) -or $p.ExitCode){throw 'Normal exit failed'}}finally{$p.Dispose()} }
  Check 'same name canary survives timeout' {$p=New-Object RelaxerVerification.NativeLease($fixture,@('20000'),$root,(Join-Path $root 'timeout.out'),(Join-Path $root 'timeout.err'),'timeout');try{$cleanup=[RelaxerVerification.Cleanup]::Run($p,50,5000);$rows.Add([ordered]@{case='timeout-target';pid=$p.Pid;created=$p.Created;image=$p.Image;command=$p.Command;parent=$p.ParentPid;root=$p.Root;cleanup=$cleanup});if($canary.Exited()){throw 'Canary touched'}}finally{$p.Dispose()} }
  Check 'no name PID or PPID adoption surface' { $methods=[RelaxerVerification.NativeLease].GetMethods().Name -join ','; if($methods-match 'OpenProcess|Adopt|Discover'){throw 'Unexpected admission API'}; if($canary.Exited()){throw 'Canary touched'} }
 } finally { $result=[RelaxerVerification.Cleanup]::Run($canary,50,5000);$rows.Add([ordered]@{case='controlled-canary-owner-cleanup';pid=$canary.Pid;created=$canary.Created;image=$canary.Image;cleanup=$result});$canary.Dispose() }
 $status='PASS'
} catch {$status='FAIL';$rows.Add([ordered]@{error=$_.Exception.ToString()});throw
} finally {[IO.File]::WriteAllText((Join-Path $root 'RESULT.json'),(@{status=$status;cases=$rows;ownership='Original CreateProcessW handles only; no descendants admitted'}|ConvertTo-Json -Depth 7))}
