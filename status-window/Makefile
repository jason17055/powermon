CC = cl

OBJECTS = main.obj hinst.obj
ADDLIB = secur32.lib shlwapi.lib gdi32.lib ole32.lib user32.lib \
		advapi32.lib \
		ws2_32.lib

all: statuswin.exe

statuswin.exe: $(OBJECTS)
	link /out:$@ $(OBJECTS) $(ADDLIB)

.c.obj:
	$(CC) /DUNICODE=1 /D_UNICODE=1 /c $<

main.obj: main.c main.h hinst.h
hinst.obj: hinst.c hinst.h

clean:
	erase $(OBJECTS)
	erase *.exe *.exe.manifest
