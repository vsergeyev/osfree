@echo off
if "%1"=="" goto main
for %%i in (%2 %3 %4 %5 %6 %7 %8 %9) do if exist %1%%i del %1%%i>nul
goto end

:main
cd suppl
call clnsuppl.bat
cd ..
call %0 .\ lastmake.mk context.h_c context.inc strings.h command.com infores
call %0 strings\ strings.h   strings.err strings.dat
call %0 strings\ strings.lib strings.lst strings.log
call %0 criter\  criter criter1 context.def context.inc context.h_c
call %0 cmd\     cmds.lib    cmds.lst
call %0 lib\     freecom.lib freecom.lst
call %0 shell\   command.exe command.map
call %0 tools\   tools.mak

call %0 strings\*.     cfg obj     exe
call %0 tools\*.   icd cfg obj map exe com
call %0 utils\*.       cfg obj map exe

for %%i in (cmd lib shell criter) do if exist %%i\*.obj del %%i\*.obj>nul
for %%i in (cmd lib shell criter) do if exist %%i\*.cfg del %%i\*.cfg>nul

:end

