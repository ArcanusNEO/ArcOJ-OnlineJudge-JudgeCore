@set MOUNTFOLDER="D:\judgecore"
@set MOUNTPOINT="/mnt/data"

@docker build -t sunrisefox/judgecore:v1.0 .
@docker run --restart always --name judgecore --cap-add=SYS_PTRACE --cap-add=SYS_ADMIN --security-opt apparmor=unconfined --security-opt seccomp=unconfined -ti -d -v %MOUNTFOLDER%:%MOUNTPOINT% --tmpfs /tmp:exec --ulimit 'stack=-1' sunrisefox/judgecore:v1.0

@REM @docker run --name judgecore --cap-add=SYS_PTRACE --cap-add=SYS_ADMIN --security-opt apparmor=unconfined --security-opt seccomp=unconfined -ti -d -v "D:\0bysj\core":/mnt/core -v "D:\0bysj\volume":/mnt/data sunrisefox/judgecore:v1.0
