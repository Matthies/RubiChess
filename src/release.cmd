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

if not exist "%ProgramFiles%\LLVM%clangver%" (
  :: Clang not installed in generic folder; search for latest version
  for /L %%v IN (30,-1,9) do (
    if exist "%ProgramFiles%\LLVM%%v" (
      set clangver=%%v
      goto foundllvm
    )
  ) 
)

:foundllvm
if exist "%ProgramFiles%\LLVM%clangver%" (
  echo Found LLVM at "%ProgramFiles%\LLVM%clangver%"
) else (
  echo Cannot find LLVM at %ProgramFiles%\LLVM*. Exit!
  goto end
)



set vcvarscmdicx="%ProgramFiles(x86)%\Intel\oneAPI\setvars.bat"
set vcvarscmd22="%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
set vcvarscmd19="%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"

set vcvarscmd=
set vsver=
if exist %vcvarscmd22% (
  set vcvarscmd=%vcvarscmd22%
  set vsver=vs2022
  echo Found Visual Studio 2022 Tools
  goto buildx64
)
if exist %vcvarscmd19% (
  set vcvarscmd=%vcvarscmd19%
  set vsver=vs2019
  echo Found Visual Studio 2019 Tools
  goto buildx64
)
echo No supported MSVC build tools found. Exit!
goto end

:buildx64
::
:: x64-64 buils
::
if exist %vcvarscmdicx% (
  echo Found Intel ICX compiler %vcvarscmdicx%
  call %vcvarscmdicx% intel64 %vsver%
  nmake -c -f Makefile.clang release COMP=icx
) else (
  echo Found MSVC Build tools %vcvarscmd%
  call %vcvarscmd% x64
  nmake -c -f Makefile.clang release COMP=clang
)

:buildarm
::
:: ARM64 build
::
call %vcvarscmd% x64_arm64
nmake -c -f Makefile.clang release COMP=clang

:end
pause

