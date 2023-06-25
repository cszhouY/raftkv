# RaftKV: A Simple Distributed Key-Value System Based On Raft

**Future**
+ C++11
+ asynchronous RPC
+ eventloop
+ RESP

**Build**
```
mkdir build && cd build
cmake ..
make
```

**Run Server**
```
./kvstoresystem --config_path ../config/server001.conf
```

**Run Client**
```
python3 ../client/client.py
```

***There are still many bugs!!! Wait for me to fix them soon.***