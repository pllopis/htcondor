# Microsoft Developer Studio Project File - Name="condor_daemon_core" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_daemon_core - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_daemon_core.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_daemon_core.mak" CFG="condor_daemon_core - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_daemon_core - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_daemon_core - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_daemon_core - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Debug"
# PROP Intermediate_Dir "..\Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\Debug\condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\Debug\condor_daemon_core.lib "

!ELSEIF  "$(CFG)" == "condor_daemon_core - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_daemon_core___Win32_Release"
# PROP BASE Intermediate_Dir "condor_daemon_core___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../Release"
# PROP Intermediate_Dir "../Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /Ob2 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"..\Release\condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "condor_daemon_core - Win32 Debug"
# Name "condor_daemon_core - Win32 Release"
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\accessdesktop.WIN32.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_daemon_core.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_ipverify.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_ipverify.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_lock.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_lock.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_lock_base.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_lock_base.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_lock_file.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_lock_file.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_lock_implementation.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_lock_implementation.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_timer_manager.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\daemon_core.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\daemon_core_main.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\exphnd.WIN32.C
# ADD CPP /Yu
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\exphnd.WIN32.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\timer_manager.C
# End Source File
# End Target
# End Project
