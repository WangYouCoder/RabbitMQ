# test:filetest.cc
test:exchangetest.cc
# test:msgqueuetest.cc
# test:bindingtest.cc
# test:messagetest.cc ../mqcommon/msg.pb.cc
# test:hosttest.cc ../mqcommon/msg.pb.cc
	g++ -o $@ $^ -lgtest -lprotobuf -lsqlite3 -std=c++11 -g
.PHONY:clean
clean:
	rm -rf test data