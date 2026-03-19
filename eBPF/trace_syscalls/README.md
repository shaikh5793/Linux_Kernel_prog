<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is licensed under GPL v2.
See the accompanying LICENSE file for the full text.
-->

# eBPF Syscall Tracer Example

## 🎯 **What This Example Demonstrates**

This example builds on the basic hello world program and shows how to trace multiple system calls using eBPF. It demonstrates several important concepts:

- **Multiple eBPF programs** in a single file
- **Different tracepoint attachments** (openat, read, write)
- **Syscall argument extraction** from tracepoint context
- **Process information gathering** (PID, TID, process name)
- **Real-world file operation monitoring**

## 📚 **Learning Objectives**

After working through this example, you'll understand:
1. How to attach eBPF programs to different syscall tracepoints
2. How to extract and interpret syscall arguments
3. How system calls like `open()` map to tracepoints (`openat`)
4. How to handle multiple eBPF programs in one application
5. Basic file I/O monitoring concepts

## 🏗️ **Project Structure**

```
trace_syscalls/
├── bpf/
│   └── trace_syscalls.bpf.c    # eBPF kernel programs (3 functions)
├── user/
│   └── trace_syscalls.c        # User-space loader and manager
├── Makefile                    # Build configuration
└── README.md                   # This file
```

## 🔧 **Building and Running**

### **1. Build the Example**
```bash
cd trace_syscalls/
make
```

### **2. Run the Tracer** 
```bash
sudo ./user/trace_syscalls
```

### **3. Monitor Output**
Open another terminal and run:
```bash
sudo cat /sys/kernel/tracing/trace_pipe
```

### **4. Generate Test Traffic**
In a third terminal, run some file operations:
```bash
ls /tmp
cat /etc/passwd  
echo "test" > /tmp/testfile
cat /tmp/testfile
rm /tmp/testfile
```

### **5. Stop Tracing**
Press `Ctrl+C` in the first terminal to stop the tracer.

## 📊 **Expected Output**

In the trace pipe, you'll see output like:
```
   trace_syscalls-1234  [001] .... 12345.678901: bpf_trace_printk: OPEN: PID=1234 TID=1234 COMM=cat dirfd=-100 flags=0
   trace_syscalls-1234  [001] .... 12345.678902: bpf_trace_printk: READ: PID=1234 TID=1234 COMM=cat fd=3 count=131072
   trace_syscalls-1234  [001] .... 12345.678903: bpf_trace_printk: WRITE: PID=1234 COMM=cat fd=1 count=1024
```

## 🔍 **Understanding the Output**

### **OPEN Events:**
- `PID`: Process ID of the program making the call
- `TID`: Thread ID (usually same as PID for single-threaded programs)
- `COMM`: Command name (limited to 15 characters)
- `dirfd`: Directory file descriptor (-100 = AT_FDCWD, current directory)
- `flags`: Open flags (0 = O_RDONLY, 1 = O_WRONLY, 2 = O_RDWR)

### **READ Events:**
- `fd`: File descriptor being read from
- `count`: Number of bytes requested to read

### **WRITE Events:**  
- `fd`: File descriptor being written to (1 = stdout, 2 = stderr)
- `count`: Number of bytes being written

## 🎓 **Key Concepts Explained**

### **1. Modern System Calls**
- `open()` → `openat()`: Modern kernels use `openat()` internally
- We trace `sys_enter_openat` instead of `sys_enter_open`

### **2. Syscall Arguments**
```c
struct trace_event_raw_sys_enter {
    struct trace_entry ent;
    long int id;                    // Syscall number
    long unsigned int args[6];      // Up to 6 syscall arguments
    char __data[0];
};
```

### **3. Multiple Program Attachment**
The skeleton automatically handles attaching all programs:
- `trace_open` → `tracepoint/syscalls/sys_enter_openat`
- `trace_read` → `tracepoint/syscalls/sys_enter_read`  
- `trace_write` → `tracepoint/syscalls/sys_enter_write`

## 🔬 **Experiment Ideas**

### **1. Filter by Process**
Modify the eBPF program to only trace specific processes:
```c
// Only trace processes named "cat"
if (strncmp(comm, "cat", 3) != 0)
    return 0;
```

### **2. Add File Descriptor Tracking**
Track which file descriptors correspond to which files.

### **3. Count System Calls**
Add eBPF maps to count how many calls each process makes.

### **4. Monitor Specific Directories**
Filter opens to only show files in specific directories (requires more advanced string handling).

## ⚠️ **Important Notes**

1. **Pointer Arguments**: Syscall arguments that are pointers (like filenames) cannot be safely dereferenced in eBPF tracepoints. Use kprobes for that.

2. **High Volume**: File I/O syscalls are very frequent. This tracer will generate a lot of output on a busy system.

3. **Performance Impact**: Tracing all syscalls has some overhead. Use filtering in production.

4. **Permissions**: Requires root privileges to load eBPF programs and read trace output.

## 🔗 **Next Steps**

This example sets the foundation for more advanced file monitoring:
- **Week 1**: Add eBPF maps for data aggregation
- **Week 2**: Use kprobes to access filename strings  
- **Week 3**: Network file operations (NFS, etc.)
- **Week 4**: Security monitoring and access control

## 🛠️ **Troubleshooting**

### **No Output in trace_pipe?**
- Check that the programs are loaded: `sudo bpftool prog list`
- Verify tracepoints exist: `ls /sys/kernel/tracing/events/syscalls/sys_enter_*`
- Try generating more file activity

### **Build Errors?**
- Ensure clang and libbpf-dev are installed
- Check that vmlinux.h was generated properly
- Verify kernel version supports BTF (5.4+)

Happy tracing! 🎉
