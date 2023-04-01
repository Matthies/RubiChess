::
:: Batch to compile all Windows release binaries
:: Needs MSVC build tools v141 for x64/x86 and arm64
::

@echo off

set SDE=
:: Some defaults for the SDE on my computer
if exist C:\bin\SDE\sde.exe (
  set SDE=C:\bin\SDE\sde.exe
  echo Found SDE at C:\bin\SDE\sde.exe
)



set vcvarscmd=%ProgramFiles(x86)%\Intel\oneAPI\setvars.bat
::if exist "%vcvarscmd%" goto buildicx

set vcvarscmd=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat
if exist "%vcvarscmd%" goto build

set vcvarscmd=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat
if exist "%vcvarscmd%" goto build

echo No supported MSVC build tools found. Exit!
goto end

:build
echo Found MSVC Build tools %vcvarscmd%

call "%vcvarscmd%" x64 -vcvars_ver=14.16
nmake -c -f Makefile.clang release COMP=clang

call "%vcvarscmd%" x64_arm64 -vcvars_ver=14.16
nmake -c -f Makefile.clang release COMP=clang
goto end

:buildicx
echo Found Intel ICX compiler %vcvarscmd%

call "%vcvarscmd%" intel64 vs2022
nmake -c -f Makefile.clang release COMP=icx

:end
pause

