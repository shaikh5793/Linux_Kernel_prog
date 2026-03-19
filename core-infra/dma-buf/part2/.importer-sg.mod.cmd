savedcmd_importer-sg.mod := printf '%s\n'   importer-sg.o | awk '!x[$$0]++ { print("./"$$0) }' > importer-sg.mod
