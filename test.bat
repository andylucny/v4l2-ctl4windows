cd v4l2-ctl\x64\Release\
v4l2-ctl
pause
v4l2-ctl --list-devices
pause
v4l2-ctl -d /dev/video0 -C zoom_absolute
pause
v4l2-ctl -d /dev/video0 -c zoom_absolute=400
pause
v4l2-ctl -d /dev/video0 -c zoom_tilt=0
pause
v4l2-ctl -d /dev/video0 -c zoom_pan=0
pause
