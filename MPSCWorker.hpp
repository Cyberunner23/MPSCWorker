/*
MIT License
Copyright (c) 2015 Alex Frappier Lachapelle


Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:


The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.


THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef MPSCWORKER_MPSCWORKER_HPP
#define MPSCWORKER_MPSCWORKER_HPP

#include <atomic>
#include <exception>
#include <memory>
#include <thread>

#include "BlockingConcurrentQueue.h"


//----------------------------------------------------------
//######################### Macros #########################
//----------------------------------------------------------

//I like having a message when something asserts.
#ifndef NDEBUG
#define assert_msg(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << ":" << __LINE__ << ": " << message << std::endl; \
            std::exit(EXIT_FAILURE); \
        } \
    } while (false)
//NOTE: the do while is so we can have a ; at the end.
#else
#define ASSERT(condition, message) do { } while (false)
#endif


//----------------------------------------------------------
//####################### Namespaces #######################
//----------------------------------------------------------

namespace Utils{
    inline const unsigned int getThreadID() noexcept {
        static std::atomic<unsigned int>       threadIDCounter(0);
        static thread_local const unsigned int threadID = threadIDCounter++;
        return threadID;
    }
}


//----------------------------------------------------------
//##################### Helper Classes #####################
//----------------------------------------------------------

template <class Type>
class SinkBase{
public:
    virtual bool onInit()             = 0;
    //NOTE: onExit unused as it causes a segmentation fault
    //NOTE:     in ConcurrentQueue's Implicit/ExplicitProducer
    //NOTE:     when onExit is called in the destructor before
    //NOTE:     the ConcurrentQueue is destroyed... WHAAAT???
    //NOTE:     see https://github.com/cameron314/concurrentqueue/issues/30
    //NOTE:     (Currently unresolved.)
    //virtual void onExit()             = 0;
    virtual void onProcess(Type data) = 0;
};


class SinkNotRegisteredException : std::exception{
public:
    SinkNotRegisteredException(unsigned int sinkID) noexcept(false) {
        this->msg  = "Sink ID: ";
        this->msg += std::to_string(sinkID);
        this->msg += " has not been registered. Use addSink"
                "to associate this ID with a Sink "
                "before starting the worker.";
    }
    const char* what() noexcept {return msg.c_str();}
    std::string whatStr() noexcept {return msg;}
private:
    std::string msg;
};


//----------------------------------------------------------
//####################### Main Class #######################
//----------------------------------------------------------

template <class Type, const unsigned int NumOfSinks>
class MPSCWorker{

public:

    MPSCWorker(){isThreadRunning.store(false);}
    ~MPSCWorker(){

        stop(true);

        //NOTE: onExit unused as it causes a segmentation fault
        //NOTE:     in ConcurrentQueue's Implicit/ExplicitProducer
        //NOTE:     when onExit is called in the destructor before
        //NOTE:     the ConcurrentQueue is destroyed... WHAAAT???
        //NOTE:     see https://github.com/cameron314/concurrentqueue/issues/30
        //NOTE:     (Currently unresolved.)
        //for(int i = 0; i <= NumOfSinks; i++){
        //    if(sinks[i]){
        //        sinks[i].get()->onExit();
        //    }
        //}
    }

    //Types
    //Type of work item
    typedef Type type;

    bool addSink(std::unique_ptr<SinkBase<Type>> &&sink, const unsigned int sinkID){
        assert_msg(sinkID < NumOfSinks,     "Error: Sink ID larger than number of allocated Sinks.");
        assert_msg(!isThreadRunning.load(), "Error: Sinks must be added before stating the worker.");
        sinks[sinkID] = std::move(sink);
        return sinks[sinkID].get()->onInit();
    }

    void start() noexcept(false) {
        isThreadRunning.store(true);
        workerThread = std::thread(run,
                                   std::ref(workQueue),
                                   std::ref(sinks),
                                   std::ref(isThreadRunning),
                                   std::ref(doFlush));
    }

    void send(Type &msg, unsigned int sinkID){
        static thread_local moodycamel::ProducerToken token(workQueue);
        if(isThreadRunning.load()){
            workQueue.enqueue(token, InternalType{msg, sinkID});
        }
    }

    void stop(bool flushQueue){
        doFlush.store(flushQueue);
        //Send an empty msg to the reserved channel to say we're done.
        Type workItem = {};
        send(workItem, NumOfSinks + 1);
        if(workerThread.joinable())
            workerThread.join();
    }


private:

    //Types
    struct InternalType{
        Type         workItem;
        unsigned int sinkID;
    };


    //Variables
    std::atomic<bool>                                 isThreadRunning;
    std::atomic<bool>                                 doFlush;
    std::thread                                       workerThread;
    std::unique_ptr<SinkBase<Type>>                   sinks[NumOfSinks];
    moodycamel::BlockingConcurrentQueue<InternalType> workQueue;


    //Functions
    static void run(moodycamel::BlockingConcurrentQueue<InternalType> &workQueue,
                    std::unique_ptr<SinkBase<Type>>                   (&sinks)[NumOfSinks],
                    std::atomic<bool>                                 &isThreadRunning,
                    std::atomic<bool>                                 &doFlush) noexcept(false) {

        moodycamel::ConsumerToken consumerToken(workQueue);

        while(isThreadRunning.load()){

            InternalType workItem = {};
            workQueue.wait_dequeue(consumerToken, workItem);

            //If we get anything with a sinkID > NumOfSinks
            //  it means we are done.
            if(workItem.sinkID > NumOfSinks){
                isThreadRunning.store(false);
                //Flush
                if(doFlush.load()){
                    while(workQueue.try_dequeue(consumerToken, workItem)){
                        if(sinks[workItem.sinkID].get() != nullptr){
                            sinks[workItem.sinkID].get()->onProcess(workItem.workItem);
                        } else
                            throw SinkNotRegisteredException(workItem.sinkID);
                    }
                }
                return;
            }

            if(sinks[workItem.sinkID].get() != nullptr)
                sinks[workItem.sinkID].get()->onProcess(workItem.workItem);
            else
                throw SinkNotRegisteredException(workItem.sinkID);
        }
    }


};

#endif //MPSCWORKER_MPSCWORKER_HPP