@echo off
@pushd	%~dp0

@COPY	/Y	Release\windesk-buoy.x86.exe		bin\
@IF	NOT	ERRORLEVEL		0			@PAUSE
@COPY	/Y	x64\Release\windesk-buoy.amd64.exe	bin\
@IF	NOT	ERRORLEVEL		0			@PAUSE

@popd
@echo	on