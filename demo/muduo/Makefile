all:dict_client dict_server
dict_client:dict_client.cc
	g++ -o $@ $^ -std=c++11 -I ./include -L ./lib -lmuduo_net -lmuduo_base -lpthread
dict_server:dict_server.cc
	g++ -o $@ $^ -std=c++11 -I ./include -L ./lib -lmuduo_net -lmuduo_base -lpthread
.PHONY:clean
clean:
	rm dict_server -f