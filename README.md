# Introduction
RDMA-Migration-System is a RDMA computing platform built to seamlessly migrate memory regions across a cluster of machines.

# Project Overview
At the moment the project is built with Make and relevant files need to be ported by the user application. 
We will change this to a library in the future.
The project is divided into the following folders:
* **doc:** relevant documentation for the project will be added to this folder
* **experiments:** micro-benchmarks for testing the system 
* **include:** header files and definition of structs and network
* **lib:** the library executable will be made in this folder when we change this to a library
* **src:** all source code declared in the include folders (currently in tpp format)
* **tests:** unit tests for all functionality

# Getting Started
## Dependencies
rdma-migration-system is C++11 project that also requires the rdma-cma library for its networking stack
the container library is built using gcc 5, to correctly configure this please install gcc 5.4.0
To configure C++11 on the syn cluster, run:
``
	sudo update-alternatives --install /usr/bin/gcc gcc /home/share/gcc/gcc5/bin/gcc 50 --slave /usr/bin/g++ g++ /home/share/gcc/gcc5/bin/g++
	export LD_LIBRARY_PATH=/home/share/gcc/gcc5/lib64
``

For RDMA migratable containers, we need to run the system with an unlimited pinned and core memory.
To make this change for a single session, you may run:
``
	sudo su
	ulimit -c unlimited
	ulimit -l unlimited
	sudo su [user_account]
``

For fault-tolerance mode, please zookeeper:
Zookeeper dependencies:

sudo apt-get install libcppunit-dev
sudo apt-get install python-setuptools
sudo apt-get install ant

clone zookeeper, for this project we are using zookeeper-3.4.11

on zookeeper top level directory:
sudo ant
sudo ant deb
sudo ant compile_jute
cd zookeeper-3.x.x/src/c
add subdir-objects to AM_INIT_AUTOMAKE in configure.ac
autoreconf -if
./configure
make 
make install

for more inforamtion please see: https://github.com/apache/zookeeper/tree/master/src/c

for only client library/bindings, simpy do:

sudo apt-get install libzookeeper-mt2




## Building
To run a particular functionality you must use Make in a particular test or experiment
Running `make` will produce the executables outlined in "test or experiment" folder
The executable server must be started in the order specified by the configuration file

## 1000 ft overview of the codebase
The codebase is built in modular fashion in include and src
* **utils:** utility functionality for the entire project, includes the preprocessor configurations of enabling paging
* **rdma-network:** the entire rdma functionality with the initial client-server design 
* **distributed-allocator:** (pseudo) distributed allocated that allocates rdma-able regions of memory to processes, it is divided into "RDMAMemory" and "RDMAMemNodes", the allocation is provided by RDMAMemory while the coordination will be performed by the RDMAMemNodes
* **paging:** The paging code to pull in memory on demand or pre-fetch
* **c++-containers:** STL containers that have been built on the migratable memory, currently we support vectors and unordered-maps

##configurations
* **LOG_LEVEL:** level of log output, ERROR, ASSERTION, WARNING, COMMENT can be asjusted from include/utils/mscutils.hpp
* **PAGING:** can we switched on with flag set to 1 (0 for no paging)
