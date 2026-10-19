#!/usr/bin/env python3
"""Measure Reader hide/show in a Niri session using isolated temporary data."""
import os,tempfile,subprocess,socket,time,json,statistics,pathlib,select,argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('binary')
parser.add_argument('--control-binary', help='measure a shortcut client instead of direct IPC')
parser.add_argument('--via-niri', action='store_true', help='include compositor spawn dispatch')
args=parser.parse_args()
if args.via_niri and not args.control_binary:parser.error('--via-niri requires --control-binary')
binary=str(pathlib.Path(args.binary).resolve())
with tempfile.TemporaryDirectory(prefix='reader-bench-') as tmp:
 root=pathlib.Path(tmp);runtime=root/'runtime';runtime.mkdir(mode=0o700)
 env=os.environ.copy();env.update(XDG_DATA_HOME=str(root/'data'),XDG_CONFIG_HOME=str(root/'config'),XDG_RUNTIME_DIR=str(runtime),WAYLAND_DISPLAY=os.path.join(os.environ['XDG_RUNTIME_DIR'],os.environ['WAYLAND_DISPLAY']),QT_QPA_PLATFORM='wayland',QT_STYLE_OVERRIDE='Fusion')
 book=root/'book.txt';book.write_text('第1章 测试\n'+'测试正文。\n'*20000)
 events=socket.socket(socket.AF_UNIX);events.connect(os.environ['NIRI_SOCKET']);events.sendall(b'"EventStream"\n');buf=b'';wins={}
 log=open(root/'stderr','w');p=subprocess.Popen([binary,str(book)],env=env,stdout=log,stderr=log)
 def event():
  global buf
  end=time.monotonic()+4
  while b'\n' not in buf:
   if not select.select([events],[],[],max(0,end-time.monotonic()))[0]:raise TimeoutError()
   chunk=events.recv(1024*1024)
   if not chunk:raise RuntimeError('disconnected')
   buf+=chunk
  line,buf=buf.split(b'\n',1);return json.loads(line)
 def apply(e):
  if 'WindowsChanged' in e:
   wins.clear()
   for w in e['WindowsChanged']['windows']:wins[w['id']]=w
  if 'WindowOpenedOrChanged' in e:
   w=e['WindowOpenedOrChanged']['window'];wins[w['id']]=w
  if 'WindowClosed' in e:wins.pop(e['WindowClosed']['id'],None)
 def visible():return any(w.get('pid')==p.pid for w in wins.values())
 def wait(want):
  while visible()!=want:apply(event())
 control_processes=[]
 def command(cmd):
  if args.control_binary:
   command_args=[str(pathlib.Path(args.control_binary).resolve()),'--'+cmd]
   control_env=env.copy()
   if 'QT_STYLE_OVERRIDE' not in os.environ:control_env.pop('QT_STYLE_OVERRIDE',None)
   if args.via_niri:
    command_args=['niri','msg','action','spawn','--','env',f'XDG_RUNTIME_DIR={runtime}',f'WAYLAND_DISPLAY={env["WAYLAND_DISPLAY"]}',*command_args]
   control_processes.append(subprocess.Popen(command_args,env=control_env,stdout=log,stderr=log))
  else:
   s=socket.socket(socket.AF_UNIX);s.connect(str(runtime/'reader-control.sock'));s.sendall(cmd.encode());s.close()
 try:
  wait(True);time.sleep(.5)
  latency={'hide':[],'show':[]}
  for _ in range(12):
   for cmd,want in [('hide',False),('show',True)]:
    t=time.perf_counter();command(cmd);wait(want);latency[cmd].append((time.perf_counter()-t)*1000);time.sleep(.15)
  command('hide');wait(False);time.sleep(.3)
  def ticks():
   a=pathlib.Path(f'/proc/{p.pid}/stat').read_text().split();return int(a[13])+int(a[14])
  t=ticks();time.sleep(3);cpu=(ticks()-t)/os.sysconf('SC_CLK_TCK')/3*100
  mem={}
  for line in pathlib.Path(f'/proc/{p.pid}/smaps_rollup').read_text().splitlines():
   if line.startswith(('Rss:','Pss:')):k,v,*_=line.split();mem[k.rstrip(':')]=int(v)
  print(json.dumps({'binary':binary,'method':'Niri EventStream, 12 toggles, 20000 text lines','control_binary':args.control_binary,'via_niri':args.via_niri,'hide_median_ms':round(statistics.median(latency['hide']),2),'show_median_ms':round(statistics.median(latency['show']),2),'hidden_cpu_percent':round(cpu,2),'memory_kib':mem}))
 finally:
  for control in control_processes:
   try:control.wait(timeout=2)
   except subprocess.TimeoutExpired:control.kill();control.wait()
  events.close();p.terminate()
  try:p.wait(timeout=3)
  except:p.kill();p.wait()
