# Sensor-actuator
Laspberry pi 3 / Linux-kernel device driver programming


execution sequence
----
make (cross compile drivers in linux terminal to raspberry bi)

-->

insmod sa_driver.c (in rasberry pi3)

-->

sh mknod.sh (in rasberry pi3)

-->

./sa_app (in rasberry pi3)

-->

input 1 -> sensor activate
input 2 -> sensor deactivate
