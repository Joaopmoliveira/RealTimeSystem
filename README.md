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
    "shared_memory_name" : "MEMORY_WATCHDOG",
    "messages" : [
        {
        "message" : "gps_reading",
        "fields" : [
            {"name" : "counter", "type" : "int", "array" : 1},
            {"name" : "latitude", "type" : "double" , "array" : 1},
            {"name" : "longitude", "type" : "double" , "array" : 1},
            {"name" : "height", "type" : "double" , "array" : 1},
            {"name" : "velocity", "type" : "double" , "array" : 3},
            {"name" : "acceleration", "type" : "double" , "array" : 3},
            {"name" : "gforce", "type" : "double" , "array" : 1},
            {"name" : "orientation", "type" : "double" , "array" : 3},
            {"name" : "angular_velocity", "type" : "double" , "array" : 3},
            {"name" : "standard_deviation", "type" : "double" , "array" : 3}
        ]
        },
        {
        "message" : "grayscale_image_1",
        "fields" : [
            {"name" : "counter", "type" : "int", "array" : 1},
            {"name" : "data", "type" : "bytes", "array" : 5880000 }
        ]  
        },
        {
        "message" : "rgb_image_1",
        "fields" : [
            {"name" : "counter", "type" : "int", "array" : 1},
            {"name" : "data", "type" : "bytes", "array" : 5880000 }
        ]  
        }
        ]
}
```

The compiler takes the name of the shared memory, and name of the messages and automatically computes the necessary size for the shared memory, the adresses on this shared memory and methods to read and write our messages unto the shared memory.

## WatchDog

The watchdog is the process which guaraantees that the sample time is respected irregardless of the workload. To do this we use assyncronous calls as much as possible and set a timer to expire at a latter point in time. If the control loop
1. Command Control Law
2. Request readings to shared memory
3. Read readings into shared memory
4. Warn client code of available readings
5. Receive control actions 

does not finish before the timer expires the watchdog executes a safety stop guarantying that the sample time is always respected.


## Client

The client code is oblivious to the timing requirements. It works on a best effort basis, trying to execute as fast as possible the readings from the sensors, the necessary post-processing and the control law. If the sample time is not respected the application will have its sockets throwing an exception warning that the communication with the watchdog has failed.

## Sensors

The sensors code is quite involded, but once you understand the structure, it will become easier to write clean and safe code which always works. Lets go into what are the requirements of our application and how we can achieve these requirements

1. The sensor code must receive a request from the watchdog. Once this request is received it must request readings from the peripherals, i.e. cameras, gps, imus, etcs..

2. Once all the peripherals have given us their reading we must warn the watchdog that we have finished all our requirements

3. The minimum possible delay should exist between executing readings between the peripherals and writing this information to the shared memory

To achieve these requirements we propose an architecture where each peripheral interacts with a dedicated thread. This thread should execute in a loop the readings in a best effort approach. To do this we use a Sincronizer object, defined according to 

```cpp
enum class Peripheral{
    GPS_READING = 0,
    CAMERA = 1,
    COUNT = 2
};

struct Sincronizer{
    std::atomic<bool> valid = false;
    std::array<std::atomic<bool>,static_cast<int>(Peripheral::COUNT)> flags;
    std::mutex mut;
    std::condition_variable cv;
    size_t written = 0;

    template<Peripheral index>
    inline bool should_write(){
        static_assert(static_cast<int>(index)<static_cast<int>(Peripheral::COUNT),"the maximum index to read must be smaller or equal than the number of peripherals");
        if(flags[static_cast<int>(index)]){
            flags[static_cast<int>(index)] = false;
            return true;
        }
        return false;
    };

    template<Peripheral... args>
    void write(){
        constexpr size_t number_of_args = sizeof...(args);
        internal_write<args...>();
        wait<number_of_args>();
    }

    inline void wrote(){
        std::lock_guard<std::mutex> g{mut};
        ++written;
        cv.notify_one();
    };

    inline void stop(){
        valid.store(true,std::memory_order_relaxed);
    };

    inline bool is_stoped(){
        return valid.load(std::memory_order_relaxed);
    };

private:
    template<Peripheral index,Peripheral... args>
    void internal_write(){
        static_assert(index!=Peripheral::COUNT,"COUNT is not a valid peripheral, it is used for internal purpouses");
        flags[static_cast<int>(index)] = true;
        if constexpr (sizeof...(args)>0)
            internal_write<args...>();
    };

    template<size_t number_of_args>
    inline void wait(){
        std::unique_lock<std::mutex> lk{mut};
        cv.wait(lk,[this](){return written == number_of_args;});
        written = 0;
    };

};

```

Although this class looks and feels convoluted, it is actually simple to use, with strong guarantees about safety. Here is a simple example showcasing how this class can be used. 

```cpp
int gps_reader(){

};


int camera_reader(){

};

int main(){
    return 0;
};
```