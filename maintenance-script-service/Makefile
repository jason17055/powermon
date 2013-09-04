CC = cl

OBJECTS = main.obj install.obj perror.obj printf.obj logger.obj \
	launch_in_session.obj
ADDLIB = secur32.lib shlwapi.lib gdi32.lib ole32.lib user32.lib \
		advapi32.lib \
		wtsapi32.lib

all: maintenance-script-service.exe

maintenance-script-service.exe: $(OBJECTS)
	link /out:$@ $(OBJECTS) $(ADDLIB)

.c.obj:
	$(CC) /c $<

main.obj: main.c main.h config.h install.h perror.h launch_in_session.h
install.obj: install.c install.h config.h perror.h
perror.obj: perror.c perror.h
logger.obj: logger.c logger.h printf.h
printf.obj: printf.c printf.h
launch_in_session.obj: launch_in_session.c launch_in_session.h

clean:
	erase $(OBJECTS)
	erase *.exe *.exe.manifest
