savedcmd_dalton_drv.mod := printf '%s\n'   dalton_drv.o | awk '!x[$$0]++ { print("./"$$0) }' > dalton_drv.mod
