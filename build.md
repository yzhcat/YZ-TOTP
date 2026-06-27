
## build

1. create build tool 'nob'

``` sh
gcc nob.c -o nob
```

2. run nob to build YZ-TOTP

``` sh
# enter vs dev shell
pwsh.exe -NoExit "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\Launch-VsDevShell.ps1" -Arch x64 -HostArch x64

# run nob to build YZ-TOTP
./nob
```
