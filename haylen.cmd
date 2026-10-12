@echo off
rem Runs haylen.py from the folder of this script, wherever it is called from.
python "%~dp0haylen.py" %*
exit /b %errorlevel%
