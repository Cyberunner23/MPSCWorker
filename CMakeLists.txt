cmake_minimum_required(VERSION 3.3)
project(MPSCWorkerTest CXX)


#----------------------------------------------------------
####################### OPTIONS ###########################
#----------------------------------------------------------
option(BUILD_SHARED_LIBS "Build MPSCWorker as a shared/dynamic library." OFF)


#----------------------------------------------------------
##################### COMPILER FLAGS ######################
#----------------------------------------------------------
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fsanitize=address -fno-omit-frame-pointer")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g2")
if(UNIX OR CYGWIN OR MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread")
endif()


#----------------------------------------------------------
#################### SOURCE / HEADERS #####################
#----------------------------------------------------------
set(MPSCWORKER_SRC_FILES
    Test/main.cpp
)
set(MPSCWORKER_HDR_FILES
    MPSCWorker.hpp
    ConcurrentQueue.h
    BlockingConcurrentQueue.h
)
include_directories(".")


#----------------------------------------------------------
######################## EXECUTABLE #######################
#----------------------------------------------------------
add_executable(MPSCWorker ${MPSCWORKER_SRC_FILES} ${MPSCWORKER_HDR_FILES})