# publish_client:publish_client.cc ../third/include/muduo/proto/codec.cc ../mqcommon/msg.pb.cc ../mqcommon/proto.pb.cc
# 	g++ -std=c++11 $@ $^ -I../third/include -L../third/lib -lmuduo_net -lmuduo_base -pthread -lprotobuf -lz
# .PHONY:clean
# clean:
# 	rm -f publish_client
CFLAG= -I../third/include
LFLAG= -L../third/lib -lgtest -lprotobuf -lsqlite3 -pthread -lmuduo_net -lmuduo_base -lz
.PHONY:publish_client consumer_client
all:publish_client consumer_client
consumer_client:consumer_client.cc ../mqcommon/msg.pb.cc ../mqcommon/proto.pb.cc ../third/include/muduo/proto/codec.cc
	g++ -g -std=c++11 $(CFLAG) $^ -o $@ $(LFLAG)
publish_client:publish_client.cc ../mqcommon/msg.pb.cc ../mqcommon/proto.pb.cc ../third/include/muduo/proto/codec.cc
	g++ -g -std=c++11 $(CFLAG) $^ -o $@ $(LFLAG)
.PHONY:clean
clean:
	rm -f publish_client consumer_client