all: sievetest spt generate1 generate2 asim1 stuff fuh sample.dat

sievetest: sievetest.cpp
	g++ -g -o $@ -O1 -I primesieve/include sievetest.cpp primesieve/libprimesieve.so

spt: spt.cpp wio.cpp Makefile
	g++ -o $@ -static -O3 -I ~/boinc-src/api -I ~/boinc-src/lib -I ~/boinc-src -I primesieve/include spt.cpp primesieve/libprimesieve.a ~/boinc-src/api/libboinc_api.a ~/boinc-src/lib/libboinc.a -lpthread

stuff: stuff.cpp wio.cpp
	g++ -g -o $@ -O0 -I primesieve/include -I ~/boinc-src/api -I ~/boinc-src/lib -I ~/boinc-src stuff.cpp primesieve/libprimesieve.a ~/boinc-src/api/libboinc_api.a ~/boinc-src/lib/libboinc.a -lpthread

generate1: generate1.cpp ../bocom/Stream.cpp ../bocom/Wiodb.cpp wio.cpp Makefile
	g++ -o $@ -g -v -O0 -I .. -I .. -I ~/boinc-src/api -I ~/boinc-src/lib -I ~/boinc-src/tools -I ~/boinc-src/sched -I ~/boinc-src/db -I /usr/include/mysql -I ~/boinc-src generate1.cpp ~/boinc-src/sched/libsched.a ~/boinc-src/api/libboinc_api.a ~/boinc-src/lib/libboinc.a ~/boinc-src/lib/libboinc_crypt.a -lmysqlclient -lpthread -lssl -lcrypto

generate2: generate2.cpp ../bocom/Stream.cpp ../bocom/Wiodb.cpp wio.cpp Makefile
	g++ -o $@ -g -O0 -I .. -I .. -I ~/boinc-src/api -I ~/boinc-src/lib -I ~/boinc-src/tools -I ~/boinc-src -I ~/boinc-src/sched -I ~/boinc-src/db -I /usr/include/mysql generate2.cpp ~/boinc-src/sched/libsched.a ~/boinc-src/api/libboinc_api.a ~/boinc-src/lib/libboinc.a ~/boinc-src/lib/libboinc_crypt.a -lmysqlclient -lpthread -lssl -lcrypto

asim1: asim.cpp ../bocom/Stream.cpp ../bocom/Wiodb.cpp wio.cpp Makefile
	g++ -o $@ -g -O1 -I primesieve/include -I .. -I .. -I ~/boinc-src/api -I ~/boinc-src/lib -I ~/boinc-src/tools -I ~/boinc-src/sched -I ~/boinc-src/db -I /usr/include/mysql -I ~/boinc-src asim.cpp ~/boinc-src/sched/libsched.a ~/boinc-src/api/libboinc_api.a ~/boinc-src/lib/libboinc.a ~/boinc-src/lib/libboinc_crypt.a  primesieve/libprimesieve.a -lmysqlclient -lpthread -lssl -lcrypto

sample.dat: stuff
	./stuff -s

output.dat: sample.dat spt
	cp sample.dat input.dat
	./spt

fuh: file_upload_handler.cpp Makefile
	g++ -o $@ -g -O0 -I .. -I .. -I ~/boinc-src/api -I ~/boinc-src/lib -I ~/boinc-src/tools -I ~/boinc-src/sched -I ~/boinc-src/db -I /usr/include/mysql -I ~/boinc-src -I /usr/include/boinc -D_USING_FCGI_ file_upload_handler.cpp ~/boinc-src/sched/fcgi_file_upload_handler-sched_config.o ~/boinc-src/sched/libsched.a ~/boinc-src/api/libboinc_api.a ~/boinc-src/lib/libboinc_fcgi.a ~/boinc-src/lib/libboinc_crypt.a -lmysqlclient -lpthread -lssl -lcrypto -lfcgi
