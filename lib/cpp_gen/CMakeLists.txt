#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements. See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership. The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the License for the
# specific language governing permissions and limitations
# under the License.
#
cmake_minimum_required(VERSION 3.20)
project(Thrift)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ")

include_directories(src src/thrift $ENV{GEN_EXTLIB}/include/boost/1_78/)

# Create the thrift C++ library
set(thriftcpp_SOURCES
	src/thrift/TApplicationException.cpp
	src/thrift/TOutput.cpp

	src/thrift/concurrency/ThreadManager.cpp
	src/thrift/concurrency/TimerManager.cpp
	src/thrift/concurrency/Util.cpp
	src/thrift/concurrency/BoostThreadFactory.cpp
	src/thrift/concurrency/BoostMonitor.cpp
	src/thrift/concurrency/BoostMutex.cpp

	src/thrift/processor/PeekProcessor.cpp
	src/thrift/protocol/TBase64Utils.cpp
	src/thrift/protocol/TDebugProtocol.cpp
	src/thrift/protocol/TJSONProtocol.cpp
	src/thrift/protocol/TMultiplexedProtocol.cpp
	src/thrift/protocol/TProtocol.cpp

	src/thrift/transport/TTransportException.cpp
	src/thrift/transport/TFDTransport.cpp
	src/thrift/transport/TSimpleFileTransport.cpp
	src/thrift/transport/TTransportUtils.cpp
	src/thrift/transport/TBufferTransports.cpp
	src/thrift/transport/TFileTransport.cpp
)
add_definitions("-D__STDC_LIMIT_MACROS")

add_library(thrift_gen ${thriftcpp_SOURCES})
