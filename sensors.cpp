#include <array>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <csignal>
#include <thread>
#include <sstream>
#include <asio.hpp>
#include <cmath>
#include <type_traits>

struct printer{
    std::mutex mut;
    template<typename T>
    printer& operator << (T obj){
        std::lock_guard<std::mutex> g{mut};
        std::cout << obj;
        return *(this);
    }
};

printer _cout;

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

void camera_reader(Sincronizer& sincronizer,std::chrono::steady_clock::time_point begin){
    while(!sincronizer.is_stoped()){
        if(sincronizer.should_write<Peripheral::CAMERA>()){
            sincronizer.wrote();
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            std::stringstream ss;
            ss << "Camera = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
            _cout << ss.str();
        }
    }
}

void gps_reader(Sincronizer& sincronizer,std::chrono::steady_clock::time_point begin){
    while(!sincronizer.is_stoped()){
        // always polling for new data
        if(sincronizer.should_write<Peripheral::GPS_READING>()){
            sincronizer.wrote();
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            std::stringstream ss;
            ss << "GPS = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
            _cout << ss.str();
        }
    }
}

Sincronizer sincronizer;

void signal_handler(int val)
{
    sincronizer.stop();
}

int main(){
    std::signal(SIGINT,signal_handler);

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    std::thread camera_thread{[&](){camera_reader(sincronizer, begin);}};
    std::thread gps_thread{[&](){gps_reader(sincronizer, begin);}};
    try{
   for(size_t counter = 0;!sincronizer.is_stoped(); ++counter){
        if(counter % 5 == 0){
            sincronizer.write<Peripheral::CAMERA,Peripheral::GPS_READING>();
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            std::stringstream ss;
            ss << "main = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
            _cout << ss.str();
        } else {
            sincronizer.write<Peripheral::GPS_READING>();
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            std::stringstream ss;
            ss << "main = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;
            _cout << ss.str();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        _cout << "=============================================\n";
    
    }
    }catch(...){
        std::cout << "failure was detected in either communication or shared memory operation\n";
    }
 
    camera_thread.join();
    gps_thread.join();
}