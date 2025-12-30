@echo off
setlocal
cd /d "%~dp0"


:: CTIME Begin ::
call "Tools/CTIME/ctime" -begin Tools/CTIME/Build.ctm


:: Unpack arguments.
for %%a in (%*) do set "%%a=1"
if not "%release%" == "1" set debug=1
if not "%pch%" == "1" set pch=0


:: Set compiler flags.
set CompilerFlagsCommon=/nologo /std:c++14 /FC /Zi /D_CRT_SECURE_NO_WARNINGS /EHsc /utf-8
set CompilerFlagsDebug=/Od /DBUILD_DEBUG=1
set CompilerFlagsRelease=/O2 /DBUILD_RELEASE=1
set CompilerFlagsWarning=/W4

:: Set linker flags
set LinkerFlagsCommon=/incremental:no /opt:ref
set LinkerFlagsDebug=/DEBUG

if "%debug%" == "1" (
    echo [Debug Build]
    set CompilerFlags=%CompilerFlagsCommon% %CompilerFlagsDebug% %CompilerFlagsWarning%
    set LinkerFlags=%LinkerFlagsCommon% %LinkerFlagsDebug%
)

if "%release%" == "1" (
    echo [Release Build]
    set CompilerFlags=%CompilerFlagsCommon% %CompilerFlagsRelease% %CompilerFlagsWarning%
    set LinkerFlags=%LinkerFlagsCommon%
)


pushd Bin
if "%pch%" == "1" (
    echo Precompiling header..
    call cl %CompilerFlags% /c /Yc ../Source/CE_PCH.cpp /link %LinkerFlags%
)

if "%pch%" == "0" (
    echo Building renderer..
    call cl %CompilerFlags% /I../Source/ThirdParty/D3D12_SDK /FI../Source/CE_PCH.h /Yu../Source/CE_PCH.h ../Source/Win32.cpp CE_PCH.obj /Fe:CE.exe /link %LinkerFlags%
)
popd


:: CTIME End ::
call "Tools/CTIME/ctime" -end Tools/CTIME/Build.ctm
