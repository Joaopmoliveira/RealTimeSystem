## Real Time System

To mimic a real time system we must have the capability of measuring time without blocking actions. To this end we
create three processes with distinct obligations. The first process is the sensors.cpp executable which reads messages from 
the peripherals and writes them somewhere (more on this later). The second process is the watchdog.cpp which guarantees 
that the desired sample time is always obeyed. The last process is the client.cpp process which does the actual computation 
in a best effort case. This is important because for the users of our real time system, their code is simple, syncronous code, 
and they have no requirerments about thinking on sample times and other characteristics. 

Now there is an open question on how should we communicate between all of these processes? A simple solution is to use 
something like Protobuf to serialize and deserialize messages and send these messages through sockets. The problem with
this approach is that for larger message sizes (aka messages with embbeded images) the size of these memory blocks can 
turn into a bottleneck when usings sockets. So we need a solution which avoids this bottle neck

## Intraprocess Comunication

Incomes the fastest solution of Intraprocess Communication, Shared Memory. Using the Intraprocess Boost library we can request 
a block of memory to be shared across these three processes. To avoid errors I implemented a compiler which receives a json 
file describing the message which must be shared accross processes.

```json
{
    "shared_memory_name" : "MyCustomName",
    "messages" : [
        {
        "message" : "gps_reading",
        "fields" : [
            {"name" : "position", "type" : "double", "array" : 3},
            {"name" : "error_state", "type" : "size_t" , "array" : 1},
            {"name" : "image", "type" : "bytes" , "array" : 1000},
        ]
        }
    ]
}
```

## WatchDog

## Client

## Sensors