::
:: Batch to compile all Windows release binaries
:: Needs MSVC build tools v141 for x64/x86 and arm64
::

@echo off

set SDE=
:: Some defaults for the SDE on my computer
if exist C:\bin\SDE\sde.exe set SDE=C:\bin\SDE\sde.exe

set vcvarscmd=
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" set vcvarscmd=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat

call "%vcvarscmd%" x64 -vcvars_ver=14.16
nmake -f Makefile.clang release

call "%vcvarscmd%" x64_arm64 -vcvars_ver=14.16
nmake -f Makefile.clang release

pause
