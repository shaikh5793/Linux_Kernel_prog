savedcmd_importer-sg-multi.mod := printf '%s\n'   importer-sg-multi.o | awk '!x[$$0]++ { print("./"$$0) }' > importer-sg-multi.mod
