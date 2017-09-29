!IF "$(PLATFORM)"=="X64" || "$(PLATFORM)"=="x64"
ARCH=amd64
!ELSE
ARCH=x86
!ENDIF

OPENCV_SRC_DIR=G:\10_Git\opencv
OPENCV_BUILD_DIR=G:\10_Git\opencv-build\dynamic-world\$(ARCH)

OUTDIR=..\$(ARCH)
OBJDIR=obj\$(ARCH)
STUBDIR=stub\$(ARCH)
SRCDIR=src

CC=cl
RD=rd /s /q
RM=del /q
LINKER=link
TARGET=curvecore.dll

OBJS=\
	$(OBJDIR)\curve_c.obj\
	$(OBJDIR)\curve_s.obj\
	$(OBJDIR)\bitmap.obj\
	$(OBJDIR)\blob.obj\
	$(OBJDIR)\container.obj\
	$(OBJDIR)\diff.obj\
	$(OBJDIR)\diff_opencv.obj\
	$(OBJDIR)\dll_export.obj\
	$(OBJDIR)\dllmain.obj\
	$(OBJDIR)\eventsink.obj\
	$(OBJDIR)\filemapping.obj\
	$(OBJDIR)\gdiscale.obj\
	$(OBJDIR)\globalcontext.obj\
	$(OBJDIR)\mainwindow.obj\
	$(OBJDIR)\olesite.obj\
	$(OBJDIR)\rpc_methods.obj\
	$(OBJDIR)\synchronization.obj\

LIBS=\
	rpcrt4.lib\
	gdi32.lib\
	opencv_world330.lib\

# warning C4100: unreferenced formal parameter
CFLAGS=\
	/nologo\
	/c\
	/DUNICODE\
	/DCURVECORE_EXPORTS\
	/O2\
	/W4\
	/Zi\
	/EHsc\
	/Fo"$(OBJDIR)\\"\
	/Fd"$(OBJDIR)\\"\
	/wd4100\
	/I$(OPENCV_BUILD_DIR)\
	/I$(OPENCV_SRC_DIR)\modules\core\include\
	/I$(OPENCV_SRC_DIR)\modules\imgproc\include\
	/I$(OPENCV_SRC_DIR)\modules\imgcodecs\include\
	/I$(OPENCV_SRC_DIR)\modules\highgui\include\
	/I$(OPENCV_SRC_DIR)\modules\videoio\include\
	/I$(STUBDIR)\

LFLAGS=\
	/NOLOGO\
	/DEBUG\
	/SUBSYSTEM:WINDOWS\
	/DLL\
	/LIBPATH:$(OPENCV_BUILD_DIR)\lib\Release\

all: $(OUTDIR)\$(TARGET)

$(OUTDIR)\$(TARGET): $(OBJS)
	@if not exist $(OUTDIR) mkdir $(OUTDIR)
	$(LINKER) $(LFLAGS) $(LIBS) /PDB:"$(@R).pdb" /OUT:$@ $**

{$(SRCDIR)}.cpp{$(OBJDIR)}.obj:
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $<

{$(STUBDIR)}.c{$(OBJDIR)}.obj:
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $<

$(STUBDIR)\curve_s.c: $(SRCDIR)\curve.idl
	@if not exist $(STUBDIR) mkdir $(STUBDIR)
	midl /nologo /prefix client c_ /prefix server s_ /h curve_rpc.h /out $(STUBDIR) $?

$(STUBDIR)\curve_c.c: $(SRCDIR)\curve.idl
	@if not exist $(STUBDIR) mkdir $(STUBDIR)
	midl /nologo /prefix client c_ /prefix server s_ /h curve_rpc.h /out $(STUBDIR) $?

clean:
	@if exist $(STUBDIR) $(RD) $(STUBDIR)
	@if exist $(OBJDIR) $(RD) $(OBJDIR)
	@if exist $(OUTDIR)\$(TARGET) $(RM) $(OUTDIR)\$(TARGET)
	@if exist $(OUTDIR)\$(TARGET:dll=ilk) $(RM) $(OUTDIR)\$(TARGET:dll=ilk)
	@if exist $(OUTDIR)\$(TARGET:dll=exp) $(RM) $(OUTDIR)\$(TARGET:dll=exp)
	@if exist $(OUTDIR)\$(TARGET:dll=lib) $(RM) $(OUTDIR)\$(TARGET:dll=lib)
	@if exist $(OUTDIR)\$(TARGET:dll=pdb) $(RM) $(OUTDIR)\$(TARGET:dll=pdb)
