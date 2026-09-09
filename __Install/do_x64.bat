rem C:\Apps\NSIS3\makensis.exe /DSQLTOOLSVER=20b%1 /DSQLTOOLS_BUILDDIR=Release.x64 SQLTools_x64.nsi
C:\Apps\NSIS3\makensis.exe /DSQLTOOLSVER=20b%1 /DSQLTOOLS_BUILDDIR=Release.x64 /DNOADMIN=1 SQLTools_x64.nsi

rmdir /S /Q SQLTools_20b%1_x64
mkdir SQLTools_20b%1_x64
copy ..\SQLHelp\SqlQkRef.chm			SQLTools_20b%1_x64
copy ..\_output_\Release.x64\SQLTools.chm	SQLTools_20b%1_x64\SQLTools_x64.chm
copy ..\_output_\Release.x64\SQLTools.exe	SQLTools_20b%1_x64\SQLTools_x64.exe
copy ..\SQLTools\ReadMe.txt			    SQLTools_20b%1_x64
copy ..\SQLTools\License.txt			SQLTools_20b%1_x64
copy ..\SQLTools\History.txt			SQLTools_20b%1_x64

mkdir SQLTools_20b%1_x64\Launcher
copy ..\sqlt-launcher\sqlt-launcher.exe .\SQLTools_20b%1_x64\Launcher
copy ..\sqlt-launcher\sqlt-launcher.xml .\SQLTools_20b%1_x64\Launcher

pkzip25 -dir -max -add SQLTools_20b%1_x64 SQLTools_20b%1_x64\*.*
rmdir /S /Q SQLTools_20b%1_x64