CC = cl

OBJECTS = main.obj install.obj perror.obj printf.obj logger.obj hinst.obj
ADDLIB = secur32.lib shlwapi.lib gdi32.lib ole32.lib user32.lib \
		advapi32.lib \
		wtsapi32.lib

all: mcpowermon.exe

mcpowermon.exe: $(OBJECTS)
	link /out:$@ $(OBJECTS) $(ADDLIB)

.c.obj:
	$(CC) /DUNICODE=1 /D_UNICODE=1 /c $<

main.obj: main.c main.h config.h install.h perror.h
install.obj: install.c install.h config.h perror.h
perror.obj: perror.c perror.h
logger.obj: logger.c logger.h printf.h
printf.obj: printf.c printf.h
hinst.obj: hinst.c hinst.h

clean:
	erase $(OBJECTS)
	erase *.exe *.exe.manifest
