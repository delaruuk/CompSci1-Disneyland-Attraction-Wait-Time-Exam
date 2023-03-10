#include <functional>
#include <chrono>
#include <future>
#include <queue>
#include <thread>
#include <memory>
#include <sstream>
#include <assert.h>

struct function_timer
{
    std::function<void()> func;
    std::chrono::system_clock::time_point time;

    function_timer()
    //      :func(nullptr),time(nullptr)
    { }

    function_timer(std::function<void()>&& f, const std::chrono::system_clock::time_point& t)
            : func(f),
              time(t)
    { }

    //Note: we want our priority_queue to be ordered in terms of
    //smallest time to largest, hence the > in operator<. This isn't good
    //practice - it should be a separate struct -  but I've done this for brevity.
    bool operator<(const function_timer& rhs) const
    {
        return time > rhs.time;
    }

    void get()
    {
        //won't work correctly
        //              std::thread t(func);
        //              t.detach();
        func();
    }
    void operator()(){
        func();
    }
};

class Scheduler
{
private:
    std::priority_queue<function_timer> tasks;
    std::unique_ptr<std::thread> thread;
    bool go_on;

    Scheduler& operator=(const Scheduler& rhs) = delete;
    Scheduler(const Scheduler& rhs) = delete;

public:

    Scheduler()
            :go_on(true),
             thread(new std::thread([this]() { ThreadLoop(); }))
    { }

    ~Scheduler()
    {
        go_on = false;
        thread->join();
    }

    void ScheduleAt(const std::chrono::system_clock::time_point& time, std::function<void()>&& func)
    {
        std::function<void()> threadFunc = [func]()
        {
            std::thread t(func);
            t.detach();
            //              func();

        };
        tasks.push(function_timer(std::move(threadFunc), time));
    }

    void ScheduleEvery(std::chrono::system_clock::duration interval, std::function<void()> func)
    {
        std::function<void()> threadFunc = [func]()
        {
            std::thread t(func);
            t.detach();
            //              func();

        };
        this->ScheduleEveryIntern(interval, threadFunc);
    }

    //in format "%s %M %H %d %m %Y" "sec min hour date month year"
    void ScheduleAt(const std::string &time, std::function<void()> func)
    {
        if (time.find("*") == std::string::npos && time.find("/") == std::string::npos)
        {
            std::tm tm = {};
            strptime(time.c_str(), "%s %M %H %d %m %Y", &tm);
            auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            if(tp>std::chrono::system_clock::now())
                ScheduleAtIntern(tp, std::move(func));
        }
    }
private:
    void ScheduleAtIntern(const std::chrono::system_clock::time_point& time, std::function<void()>&& func)
    {
        tasks.push(function_timer(std::move(func), time));
    }

    void ScheduleEveryIntern(std::chrono::system_clock::duration interval, std::function<void()> func)
    {
        std::function<void()> waitFunc = [this,interval,func]()
        {
            //              std::thread t(func);
            //              t.detach();
            func();
            this->ScheduleEveryIntern(interval, func);
        };
        ScheduleAtIntern(std::chrono::system_clock::now() + interval, std::move(waitFunc));
    }

    void ThreadLoop()
    {
        while(go_on)
        {
            auto now = std::chrono::system_clock::now();
            while(!tasks.empty() && tasks.top().time <= now) {
                function_timer f = tasks.top();
                f.get();
                tasks.pop();
            }

            if(tasks.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else {
                std::this_thread::sleep_for(tasks.top().time - std::chrono::system_clock::now());
            }
        }
    }

};