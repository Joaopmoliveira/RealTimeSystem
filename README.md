## Real Time System

To mimic a real time system we must have the capability of measuring time without blocking actions. To this end we
create three processes with distinct obligations. The first process is the sensors.cpp executable which reads messages from 
the peripherals and writes them somewhere (more on this later). The second process is the watchdog.cpp which guarantees 
that the desired sample time is always obeyed. The last process is the client.cpp process which does the actual computation 
in a best effort case. This is important because for the users of our real time system, their code is simple, syncronous code, 
and they have no requirerments about thinking on sample times and other characteristics. 