# test:filetest.cc
# test:exchangetest.cc
# test:msgqueuetest.cc
# test:bindingtest.cc
# test:messagetest.cc ../mqcommon/msg.pb.cc
# test:hosttest.cc ../mqcommon/msg.pb.cc
# test:routetest.cc
# test:consumertest.cc
# 	g++ -o $@ $^ -lgtest -lprotobuf -lsqlite3 -std=c++11 -g
# test:channeltest.cc ../mqcommon/msg.pb.cc ../mqcommon/proto.pb.cc
# test:connectiontest.cc ../mqcommon/msg.pb.cc ../mqcommon/proto.pb.cc
# 	g++ -o $@ $^ -lgtest -lprotobuf -lsqlite3 -std=c++11 -g -I../third/include
# .PHONY:clean
# clean:
# rm -rf test
CFLAG= -I../third/include
LFLAG= -L../third/lib -lgtest -lprotobuf -lsqlite3 -pthread -lmuduo_net -lmuduo_base -lz
test:brokentest.cc ../mqcommon/msg.pb.cc ../mqcommon/proto.pb.cc ../third/include/muduo/proto/codec.cc
	g++ -g -std=c++11 $(CFLAG) $^ -o $@ $(LFLAG)
.PHONY:clean
clean:
	rm -f test
