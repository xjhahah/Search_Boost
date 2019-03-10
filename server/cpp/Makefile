PROTOC=~/third_part/bin/protoc
FLAG=-g -std=c++11 -I ~/third_part/include -L ~/third_part/lib \
		 		 -L ../../index/cpp\
				 		 -lsofa-pbrpc -lprotobuf -lglog -lgflags -lpthread\
						 		 -lz -lsnappy -lindex

server:server.pb.cc server_main.cc doc_searcher.cc 
	g++ $^ -o $@ $(FLAG)
	cp -f $@ ../bin/

server.pb.cc:server.proto 
	$(PROTOC) server.proto --cpp_out=.

.PHONY:clean
clean:
	rm server server.pb.* 
