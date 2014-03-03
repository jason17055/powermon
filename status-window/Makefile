CC = cl

OBJECTS = main.obj hinst.obj printf.obj
ADDLIB = secur32.lib shlwapi.lib gdi32.lib ole32.lib user32.lib \
		advapi32.lib \
		ws2_32.lib

all: statuswin.exe status.exe watchdog.exe

statuswin.exe: $(OBJECTS)
	link /out:$@ $(OBJECTS) $(ADDLIB)

status.exe: send_main.obj
	link /out:$@ send_main.obj $(ADDLIB)

watchdog.exe: watchdog_cmd.obj
	link /out:$@ watchdog_cmd.obj $(ADDLIB)

.c.obj:
	$(CC) /DUNICODE=1 /D_UNICODE=1 /c $<

main.obj: main.c main.h hinst.h printf.h
hinst.obj: hinst.c hinst.h
printf.obj: printf.c printf.h
send_main.obj: send_main.c
watchdog_cmd.obj: watchdog_cmd.c

clean:
	erase $(OBJECTS)
	erase *.exe *.exe.manifest
