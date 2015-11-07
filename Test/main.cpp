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

#include <fstream>
#include <iostream>
#include "MPSCWorker.hpp"

template <class Type>
class PrintSink : public SinkBase<Type>{
public:
    virtual bool onInit(){return true;}
    virtual void onProcess(Type data){std::cout << data << std::endl;}
};

template <class Type>
class FileSink : public SinkBase<Type>{
public:
    virtual bool onInit(){
        fileStream.open("Test.txt");
        if(!fileStream.is_open()){return false;}
        fileStream << "There should be 5 messages after this.";
        return true;
    }
    virtual void onProcess(Type data){fileStream << data << std::endl;}
private:
    std::ofstream fileStream;
};


typedef std::string WorkItemType;


static void run(MPSCWorker<WorkItemType, 2> &worker){
    WorkItemType msg = "Thread " + std::to_string(Utils::getThreadID())
                       + ": Test message.";
    worker.send(msg, 0);
    worker.send(msg, 1);
}


int main(){

    MPSCWorker<WorkItemType, 2> worker;

    std::unique_ptr<PrintSink<WorkItemType>> printSink(new PrintSink<WorkItemType>);
    if(printSink.get() == nullptr){
        return -1;
    }

    std::unique_ptr<FileSink<WorkItemType>> fileSink(new FileSink<WorkItemType>);
    if(fileSink.get() == nullptr){
        return -1;
    }

    if(!worker.addSink(std::move(printSink), 0)){return -10;}
    if(!worker.addSink(std::move(fileSink),  1)){return -10;}

    worker.start();

    std::thread threads[4];
    for(int i = 0; i < 4; i++){
        threads[i] = std::thread(run, std::ref(worker));
    }

    for(int i = 0; i < 4; i++){
        threads[i].join();
    }

    return 0;
}