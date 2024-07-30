CFLAG= -I../third/include
LFLAG= -L../third/lib -lgtest -lprotobuf -lsqlite3 -pthread -lmuduo_net -lmuduo_base -lz
server:server.cc ../mqcommon/msg.pb.cc ../mqcommon/proto.pb.cc ../third/include/muduo/proto/codec.cc
	g++ -g -std=c++11 $(CFLAG) $^ -o $@ $(LFLAG)
.PHONY:clean
clean:
	rm -f server