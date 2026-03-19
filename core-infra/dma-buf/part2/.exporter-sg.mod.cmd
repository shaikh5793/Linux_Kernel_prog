savedcmd_exporter-sg.mod := printf '%s\n'   exporter-sg.o | awk '!x[$$0]++ { print("./"$$0) }' > exporter-sg.mod
