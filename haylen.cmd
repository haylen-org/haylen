@echo off
rem Runs the command line of Haylen, the package in "tools\haylen", through uv from the folder of this script, wherever it is called from.
where uv >nul 2>nul
if errorlevel 1 (
    echo Error: The command "uv" was not found. Install uv from https://docs.astral.sh/uv and run "haylen" again. 1>&2
    exit /b 1
)
uv run --project "%~dp0." --frozen haylen %*
exit /b %errorlevel%
