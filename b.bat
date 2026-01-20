@echo off
setlocal
cd /d "%~dp0"


:: CTIME Begin ::
call "Tools/CTIME/ctime" -begin Tools/CTIME/Build.ctm


:: Get cl.exe
where cl >nul 2>nul
if %errorlevel%==1 (
    echo Looking for 'vcvars64.bat'.. Recommended to run from the Developer Command Prompt.
    @call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

where /q cl || (
    echo [ERROR]: "cl" not found - please run this from the MSVC x64 native tools command prompt.
    exit /b 1
)

:: Unpack arguments.
for %%a in (%*) do set "%%a=1"
if not "%release%" == "1" set debug=1
if not "%pch%" == "1" set pch=0


:: Set compiler flags.
set CompilerFlagsCommon=/nologo /std:c++20 /FC /Zi /D_CRT_SECURE_NO_WARNINGS /EHsc /utf-8
set CompilerFlagsDebug=/Od /DBUILD_DEBUG=1
set CompilerFlagsRelease=/O2 /DBUILD_RELEASE=1
set CompilerFlagsWarning=/W4 /wd4456 /wd4324

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
    call cl %CompilerFlags% /c /Yc ../Source/PCH.cpp /link %LinkerFlags%
)

if "%pch%" == "0" (
    echo Building engine..
    call cl %CompilerFlags% /I../Source/ThirdParty/D3D12_SDK /FI../Source/PCH.h /Yu../Source/PCH.h ../Source/Win32.cpp PCH.obj /Fe:CE.exe /link %LinkerFlags%
)

popd


:: CTIME End ::
call "Tools/CTIME/ctime" -end Tools/CTIME/Build.ctm
