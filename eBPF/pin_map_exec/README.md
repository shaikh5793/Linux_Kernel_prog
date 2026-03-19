<!--
Copyright (c) 2024 TECH VEDA(www.techveda.org)
Author: Raghu Bharadwaj

This software is licensed under GPL v2.
See the accompanying LICENSE file for the full text.
-->

# Example — pin_map_exec

Pin a map in bpffs and read it from a separate process.

Details
- BPF program counts `execve` per `comm` into a hash map.
- Loader pins the map at `/sys/fs/bpf/exec_cnt` and keeps running.
- A separate `reader` opens the pinned map and prints counts periodically.

Build
- `make`

Run
- Terminal A (loader): `sudo ./user/pinner`
- Terminal B (reader): `sudo ./user/reader`

Cleanup
- Stop the loader; it cleans up the program. The pinned map remains until deleted:
  `sudo rm -f /sys/fs/bpf/exec_cnt`

