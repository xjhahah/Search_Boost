PROTOC=~/third_part/bin/protoc 

FLAG=-g -std=c++11 -I ~/third_part/include -L ~/third_part/lib\
		 -lsofa-pbrpc -lprotobuf -lglog -lgflags -lpthread\
		 -lz -lsnappy

client:client_main.cc server.pb.cc 
	g++ $^ -o $@ $(FLAG)
	cp -f $@ ../bin/

server.pb.cc:server.proto 
	$(PROTOC) server.proto --cpp_out=.

.PHONY:clean 
clean:
	rm server.pb.* client 

