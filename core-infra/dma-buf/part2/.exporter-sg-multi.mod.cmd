savedcmd_exporter-sg-multi.mod := printf '%s\n'   exporter-sg-multi.o | awk '!x[$$0]++ { print("./"$$0) }' > exporter-sg-multi.mod
