# Sensor-actuator
Laspberry pi 3 / Linux-kernel device driver programming
cross compile used


execution sequence
----
make (in linux terminal)

-->

insmod sa_driver.c (in rasberry pi3)

-->

sh mknod.sh (in rasberry pi3)

-->

./sa_app (in rasberry pi3)

-->

input 1 -> sensor activate
input 2 -> sensor deactivate
