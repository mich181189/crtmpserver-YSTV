
REM FOR /F "tokens=*" %%G IN ('DIR /B /S Release\*.pdb') DO echo "%%G" && del /Q "%%G"
FOR /F "tokens=*" %%G IN ('DIR /B /S Release\x64\*.lib') DO echo "%%G" && del /Q "%%G"
FOR /F "tokens=*" %%G IN ('DIR /B /S Release\x64\*.exp') DO echo "%%G" && del /Q "%%G"
xcopy /E /I /Y ..\..\sources\applications\flvplayback\flex\bin-debug Release\x64\applications\flvplayback\flex
xcopy /E /I /Y ..\..\sources\applications\mpegtsplayback\flex\bin-debug Release\x64\applications\mpegtsplayback\flex
FOR /F "tokens=*" %%G IN ('DIR /B /S Release\win32\*.lib') DO echo "%%G" && del /Q "%%G"
FOR /F "tokens=*" %%G IN ('DIR /B /S Release\win32\*.exp') DO echo "%%G" && del /Q "%%G"
xcopy /E /I /Y ..\..\sources\applications\flvplayback\flex\bin-debug Release\win32\applications\flvplayback\flex
xcopy /E /I /Y ..\..\sources\applications\mpegtsplayback\flex\bin-debug Release\win32\applications\mpegtsplayback\flex
