@setlocal
@set VCVERS=14
@set TMPDRV=X:
@if "%DOINT%x" == "x" (
@set DOINST=0
)
@set TMPPRJ=shapelib
@set TMPLOG=bldlog-1.txt
@set BUILD_RELDBG=0
@set TMPSRC=%TMPDRV%\%TMPPRJ%
@echo Build of '%TMPPRJ% in 64-bits 
@if NOT EXIST %TMPDRV%\nul goto NOXDIR
@set TMP3RD=%TMPDRV%\3rdParty.x64
@set CONTONERR=0
@set DOPAUSE=pause
@if "%~1x" == "NOPAUSEx" @set DOPAUSE=echo NO PAUSE requested

@if EXIST build-cmake.bat (
@call build-cmake.bat
)

@if NOT EXIST %TMPSRC%\CMakeLists.txt goto NOCM

@REM ############################################
@REM NOTE: MSVC 10...14 INSTALL LOCATION
@REM Adjust to suit your environment
@REM ##########################################
@set GENERATOR=Visual Studio %VCVERS% Win64
@set VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio %VCVERS%.0
@set VC_BAT=%VS_PATH%\VC\vcvarsall.bat
@if NOT EXIST "%VS_PATH%" goto NOVS
@if NOT EXIST "%VC_BAT%" goto NOBAT
@set BUILD_BITS=%PROCESSOR_ARCHITECTURE%
@IF /i %BUILD_BITS% EQU x86_amd64 (
    @set "RDPARTY_ARCH=x64"
) ELSE (
    @IF /i %BUILD_BITS% EQU amd64 (
        @set "RDPARTY_ARCH=x64"
    ) ELSE (
        @echo Appears system is NOT 'x86_amd64', nor 'amd64'
        @echo Can NOT build the 64-bit version! Aborting
        @exit /b 1
    )
)

@ECHO Setting environment - CALL "%VC_BAT%" %BUILD_BITS%
@CALL "%VC_BAT%" %BUILD_BITS%
@if ERRORLEVEL 1 goto NOSETUP

@set TMPINST=%TMP3RD%

@set TMPOPTS=
@set TMPOPTS=%TMPOPTS% -DCMAKE_INSTALL_PREFIX:PATH=%TMPINST%
@set TMPOPTS=%TMPOPTS% -G "%GENERATOR%"

@call chkmsvc %TMPPRJ%

@REM To help find Qt5
@REM Setup Qt5 - 64-bit
@REM call setupqt5.6
@REM if EXIST tempsp.bat @del tempsp.bat
@REM call setupqt564 -N
@REM if NOT EXIST tempsp.bat goto NOSP
@REM call tempsp

@echo Built project %TMPPRJ% 64-bits... all ouput to %TMPLOG%

@REM A 64-bit build of shapelib
@REM set INSTALL_DIR=%TMPDRV%\install\msvc140-64\%TMPPRJ%
@REM set SIMGEAR_DIR=%TMPDRV%\install\msvc140-64\SimGear
@REM set OSG_DIR=%TMPDRV%\install\msvc140-64\OpenSceneGraph
@REM set BOOST_ROOT=%TMPDRV%\boost_1_60_0
@REM set ZLIBDIR=%TMP3RD%


@REM if NOT EXIST %SIMGEAR_DIR%\nul goto NOSGD
@REM if NOT EXIST %ZLIBDIR%\nul goto NOZLD

@REM set PostgreSQL_ROOT=C:\Program Files (x86)\PostgreSQL\9.1

@REM if NOT EXIST %SIMGEAR_DIR%\nul goto NOSGD
@REM if "%Qt5_DIR%x" == "x" NOQTD
@REM if NOT EXIST %Qt5_DIR%\nul goto NOQTD
@REM if NOT EXIST %ZLIBDIR%\nul goto NOZLD

@set CMOPTS=%TMPOPTS%
@REM set CMOPTS=%CMOPTS% -DCMAKE_INSTALL_PREFIX=%TMP3RD%
@REM set CMOPTS=%CMOPTS% -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR%
@REM set CMOPTS=%CMOPTS% -DCMAKE_PREFIX_PATH:PATH=%ZLIBDIR%;%OSG_DIR%;%SIMGEAR_DIR%
@REM set CMOPTS=%CMOPTS% -DZLIB_ROOT=%ZLIBDIR%
@REM set CMOPTS=%CMOPTS% -DMSVC_3RDPARTY_ROOT=%TMP3RD%
@REM set CMOPTS=%CMOPTS% -DBOOST_ROOT=%BOOST_ROOT%
@REM Where is the OSG PATCH???
@REM set CMOPTS=%CMOPTS% -DOSG_FSTREAM_EXPORT_FIXED:BOOL=YES

@echo Build of '%TMPPRJ% in 64-bits being %DATE% at %TIME% > %TMPLOG%

@REM echo Set SIMGEAR_DIR=%SIMGEAR_DIR% >> %TMPLOG%
@REM echo Set QT5_DIR=%Qt5_DIR% >> %TMPLOG%
@REM echo Set ZLIBDIR=%ZLIBDIR% >> %TMPLOG%
@REM echo Set OSG_DIR=%OSG_DIR% >> %TMPLOG%
@REM echo Set BOOST_ROOT=%BOOST_ROOT% >> %TMPLOG%

@REM echo Do: 'cmake %TMPSRC% %CMOPTS%'
@REM echo.
@REM %DOPAUSE%

@echo Doing: 'cmake %TMPSRC% %CMOPTS%'
@echo Doing: 'cmake %TMPSRC% %CMOPTS% >> %TMPLOG%
@cmake %TMPSRC% %CMOPTS% >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR1
@set TMPBLDS=

@if "%BUILD_RELDBG%x" == "1x" goto DNDBGBLD
@echo Doing: 'cmake --build . --config Debug'
@echo Doing: 'cmake --build . --config Debug' >> %TMPLOG%
@cmake --build . --config Debug >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR2
@set TMPBLDS=%TMPBLDS% Debug

:DNDBGBLD
@if "%BUILD_RELDBG%x" == "0x" goto DNRELDBG
@echo Doing: 'cmake --build . --config RelWithDebInfo'
@echo Doing: 'cmake --build . --config RelWithDebInfo' >> %TMPLOG%
@cmake --build . --config RelWithDebInfo >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR3
@set TMPBLDS=%TMPBLDS% RelWithDebInfo
:DNRELDBG

@echo Doing: 'cmake --build . --config Release'
@echo Doing: 'cmake --build . --config Release' >> %TMPLOG%
@cmake --build . --config Release >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR4
@set TMPBLDS=%TMPBLDS% Release

@echo Appears successful build of %TMPBLDS%...

@if "%DOINST%x" == "1x" goto ADDINST
@echo.
@echo Install is disabled, to %SIMGEAR_DIR%.... Set DOINST=1
@echo.
@goto END

:ADDINST
@echo.
@echo ######## CONTINUE WITH INSTALL %TMPPRJ% TO %TMPINST% ########
@REM echo Will install Release only...
@echo.
@REM echo Will do: 'cmake --build . --config Release --target INSTALL'
@REM echo.
@%DOPAUSE%

@echo Doing: 'cmake --build . --config Debug --target INSTALL'
@echo Doing: 'cmake --build . --config Debug --target INSTALL' >> %TMPLOG%
@cmake --build . --config Debug --target INSTALL >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR5


@echo Doing: 'cmake --build . --config Release --target INSTALL'
@echo Doing: 'cmake --build . --config Release --target INSTALL' >> %TMPLOG%
@cmake --build . --config Release --target INSTALL >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR5

@fa4 " -- " %TMPLOG%

@echo.
@echo Done build and install... see %TMPLOG% for details...
@echo.

@goto END

:NOCM
@echo NOT EXIST %TMPSRC%\CMakeLists.txt! *** FIX ME ***
@goto ISERR

:NOSETUP
@echo MSVC setup FAILED!
@goto ISERR

:NOBAT
@echo Can not locate "%VC_BAT%"! *** FIX ME *** for your environment
@goto ISERR

:NOVS
@echo Can not locate "%VS_PATH%"! *** FIX ME *** for your environment
@goto ISERR

@REM :NOSGD
@echo Note: Simgear directory %SIMGEAR_DIR% does NOT EXIST!  *** FIX ME ***
@goto ISERR

:NOQTD
@echo Note: Qt directory '%Qt5_DIR%' does NOT EXIST! *** FIX ME ***
@goto ISERR

:NOZLD
@echo Note: ZLIB direcotry %ZLIBDIR% does NOT EXIST! *** FIX ME ***
@goto ISERR

:NOSP
@echo Failed to generate a tempsp.bat! *** FIX ME ***
@goto ISERR

:ERR1
@echo FATAL ERROR: cmake configuration/generation FAILED
@echo FATAL ERROR: cmake configuration/generation FAILED >> %TMPLOG%
@goto ISERR

:ERR2
@echo ERROR: cmake build Debug
@echo ERROR: cmake build Debug >> %TMPLOG%
@if %CONTONERR% EQU 1 goto DNDBGBLD
@goto ISERR

:ERR3
@echo ERROR: cmake build RelWithDebInfo
@echo ERROR: cmake build RelWithDebInfo >> %TMPLOG%
@if %CONTONERR% EQU 1 goto DNRELDBG
@goto ISERR

:ERR4
@echo ERROR: cmake build Release
@echo ERROR: cmake build Release >> %TMPLOG%
@goto ISERR

:ERR5
@echo ERROR: cmake install
@goto ISERR

:NOXDIR
@echo.
@echo Oops, no X: directory found! Needed for simgear, etc
@echo Run setupx, or hdem3m, etc, to establish X: drive
@echo.
@goto ISERR



:ISERR
@endlocal
@exit /b 1

:END
@endlocal
@exit /b 0

@REM eof
