# test:filetest.cc
# test:exchangetest.cc
test:msgqueuetest.cc
	g++ -o $@ $^ -lgtest -lprotobuf -lsqlite3 -std=c++11
.PHONY:clean
clean:
	rm -f test