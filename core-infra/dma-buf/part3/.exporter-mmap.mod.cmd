savedcmd_exporter-mmap.mod := printf '%s\n'   exporter-mmap.o | awk '!x[$$0]++ { print("./"$$0) }' > exporter-mmap.mod
