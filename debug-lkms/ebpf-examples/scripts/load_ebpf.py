#!/usr/bin/env python3
#
# Copyright (c) 2024 TECH VEDA(www.techveda.org)
# Author: Raghu Bharadwaj
#
# This software is licensed under GPL v2.
# See the accompanying LICENSE file for the full text.
#
"""
eBPF Program Loader for ARM VExpress
Simple loader using bpftool
"""

import subprocess
import sys
import argparse
import time

PROGRAMS = {
    'memleak': {
        'file': 'memleak_tracker.o',
        'desc': 'Memory leak tracker (kmalloc/kfree)',
    },
    'syscall': {
        'file': 'syscall_tracer.o',
        'desc': 'Syscall tracer with latency',
    },
    'functime': {
        'file': 'func_latency.o',
        'desc': 'Function latency profiler',
    },
    'locks': {
        'file': 'lock_contention.o',
        'desc': 'Lock contention analyzer',
    },
    'pagefault': {
        'file': 'pagefault_tracker.o',
        'desc': 'Page fault tracker',
    },
}

def run_cmd(cmd):
    """Run command and return output"""
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        return result.returncode, result.stdout, result.stderr
    except Exception as e:
        return -1, "", str(e)

def load_program(name, obj_file):
    """Load eBPF program using bpftool"""
    print(f"Loading {name} from {obj_file}...")
    
    cmd = f"bpftool prog load {obj_file} /sys/fs/bpf/{name} type tracing"
    rc, out, err = run_cmd(cmd)
    
    if rc == 0:
        print(f"✓ Loaded successfully")
        return True
    else:
        print(f"✗ Failed to load: {err}")
        return False

def list_programs():
    """List loaded eBPF programs"""
    print("Loaded eBPF programs:")
    rc, out, err = run_cmd("bpftool prog list")
    if rc == 0:
        print(out)
    else:
        print("No programs loaded or bpftool not available")

def show_maps(name):
    """Show maps for a program"""
    print(f"\nMaps for {name}:")
    rc, out, err = run_cmd(f"bpftool map list")
    if rc == 0:
        print(out)
    else:
        print("Could not list maps")

def main():
    parser = argparse.ArgumentParser(description='eBPF Program Loader')
    parser.add_argument('action', choices=['load', 'list', 'unload', 'available'],
                       help='Action to perform')
    parser.add_argument('program', nargs='?', help='Program name to load/unload')
    parser.add_argument('--obj-dir', default='./bin', help='Directory with .o files')
    
    args = parser.parse_args()
    
    if args.action == 'available':
        print("Available eBPF programs:")
        for name, info in PROGRAMS.items():
            print(f"  {name:12} - {info['desc']}")
        sys.exit(0)
    
    elif args.action == 'list':
        list_programs()
        sys.exit(0)
    
    elif args.action == 'load':
        if not args.program:
            print("Error: Specify program name")
            sys.exit(1)
        
        if args.program not in PROGRAMS:
            print(f"Unknown program: {args.program}")
            print("Available:", ", ".join(PROGRAMS.keys()))
            sys.exit(1)
        
        obj_file = f"{args.obj_dir}/{PROGRAMS[args.program]['file']}"
        if load_program(args.program, obj_file):
            show_maps(args.program)
    
    elif args.action == 'unload':
        if not args.program:
            print("Error: Specify program name")
            sys.exit(1)
        
        print(f"Unloading {args.program}...")
        rc, _, _ = run_cmd(f"rm /sys/fs/bpf/{args.program}")
        if rc == 0:
            print("✓ Unloaded")
        else:
            print("✗ Failed or not loaded")

if __name__ == '__main__':
    main()
