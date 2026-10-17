@echo off
rem Builds the test tools (x64) into tests\build. Run from any folder.
setlocal
set ROOT=%~dp0..
set VCPKG=%ROOT%\vcpkg_installed\x64\x64-windows-static
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -property installationPath`) do set VS=%%i
call "%VS%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul || exit /b 1
if not exist "%~dp0build" mkdir "%~dp0build"
cd /d "%~dp0build"
set FLAGS=/nologo /EHsc /O2 /MT /W3 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /I"%VCPKG%\include"
set LIBS=/link /LIBPATH:"%VCPKG%\lib" ogg.lib vorbis.lib vorbisenc.lib vorbisfile.lib FLAC.lib FLAC++.lib libwavpack.lib
cl %FLAGS% /Fe:make_test_files.exe "%~dp0make_test_files.cpp" "%ROOT%\md5.c" %LIBS% || exit /b 1
cl %FLAGS% /Fe:decoder_test.exe "%~dp0decoder_test.cpp" "%ROOT%\Decoders.cpp" "%ROOT%\MP3Decoder.cpp" "%ROOT%\OggDecoder.cpp" "%ROOT%\FlacDecoder.cpp" "%ROOT%\WvDecoder.cpp" "%ROOT%\Stream.cpp" "%ROOT%\md5.c" %LIBS% wsetargv.obj || exit /b 1
echo Test tools built in %~dp0build
