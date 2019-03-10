.PHONY:all
all:Http TestCGI

TestCGI:TestCGI.cpp
	g++ -o $@ $^

Http:Http.cpp
	g++ -o $@ $^ -std=c++11 -lpthread -g #-static #-g -fpermissive

.PHONY:clean
clean:
	rm -rf Http TestCGI output

.PHONY:output
output:
	@mkdir output
	@cp -rf wwwroot output
	@cp Httpd output
	@cp start.sh output
	@cp TestCGI output/wwwroot
