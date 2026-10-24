@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >/dev/null 2>&1
cd /d %~dp0
cl /nologo /O2 /EHsc /std:c++17 /I ..\..\..\Source /I ..\..\..\Source\Core /I ..\..\..\Source\DSP /I ..\..\..\Source\DSP\Common /I D:\desarrollos\ABDSynths\ABDShared\Code abd_filter_harness.cpp ..\..\..\Source\DSP\Filters\MultiModeFilter.cpp ..\..\..\Source\DSP\Filters\FilterResonanceComp.cpp /Fe:abd_filter_harness.exe
echo CL_EXIT=%ERRORLEVEL%
