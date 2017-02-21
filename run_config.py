#!/usr/bin/python
import sys
import os
import subprocess
from subprocess import Popen, PIPE

arguments = {}

prefix_str = ""

if(len(sys.argv) < 2):
  print("Usage : python run_config.py <executable> <config_file>")
  sys.exit()
plot_script=""

#print "opening config_file ", sys.argv[2]
f=open(sys.argv[2], 'r')

for line in f:
  if(len(line) < 3):
    continue
  str=(line.strip()).split('=')
  arg_key=str[0]
  if(arg_key[0] == '#'):
    continue;
  arg_val=str[1]
  if(arg_key == "plot_script"):
    plot_script=arg_val
    continue;
  if(arg_key == "prefix"):
    prefix_str = arg_val
    continue
  arguments[arg_key] = arg_val

final_args=""
for arg_key in arguments:
  final_args = final_args+" --"+arg_key+"=\""+arguments[arg_key]+"\""

cmd_line="./waf --run \""+sys.argv[1]+final_args+"\""

if (len(prefix_str) > 0):
  cmd_line = cmd_line + " > "+prefix_str+".out "+" 2> "+prefix_str+".err"

print(cmd_line)

subprocess.call(cmd_line, shell="False")

f.close()

