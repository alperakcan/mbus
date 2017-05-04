
subdir-y = \
	app \
	src \
	test

app_depends-y = \
	src \
	test

test_depends-y = \
	src

include Makefile.lib

install: app src test
	install -d /usr/local/bin
	install -m 0755 dist/bin/mbus-cli /usr/local/bin/mbus-cli
	install -m 0755 dist/bin/mbus-controller /usr/local/bin/mbus-controller
	install -m 0755 dist/bin/mbus-listener /usr/local/bin/mbus-listener
	
	install -d /usr/local/include/mbus
	install -m 0644 dist/include/mbus/client.h /usr/local/include/mbus/client.h
	install -m 0644 dist/include/mbus/debug.h /usr/local/include/mbus/debug.h
	install -m 0644 dist/include/mbus/exec.h /usr/local/include/mbus/exec.h
	install -m 0644 dist/include/mbus/json.h /usr/local/include/mbus/json.h
	install -m 0644 dist/include/mbus/method.h /usr/local/include/mbus/method.h
	install -m 0644 dist/include/mbus/server.h /usr/local/include/mbus/server.h
	install -m 0644 dist/include/mbus/socket.h /usr/local/include/mbus/socket.h
	install -m 0644 dist/include/mbus/tailq.h /usr/local/include/mbus/tailq.h
	
	install -d /usr/local/lib
	install -m 0755 dist/lib/libmbus-client.so /usr/local/lib/libmbus-client.so
	install -m 0755 dist/lib/libmbus-debug.so /usr/local/lib/libmbus-debug.so
	install -m 0755 dist/lib/libmbus-exec.so /usr/local/lib/libmbus-exec.so
	install -m 0755 dist/lib/libmbus-json.so /usr/local/lib/libmbus-json.so
	install -m 0755 dist/lib/libmbus-json-cJSON.so /usr/local/lib/libmbus-json-cJSON.so
	install -m 0755 dist/lib/libmbus-server.so /usr/local/lib/libmbus-server.so
	install -m 0755 dist/lib/libmbus-socket.so /usr/local/lib/libmbus-socket.so
	
	install -d /usr/local/lib
	install -m 0644 dist/lib/libmbus-client.a /usr/local/lib/libmbus-client.a
	install -m 0644 dist/lib/libmbus-debug.a /usr/local/lib/libmbus-debug.a
	install -m 0644 dist/lib/libmbus-exec.a /usr/local/lib/libmbus-exec.a
	install -m 0644 dist/lib/libmbus-json.a /usr/local/lib/libmbus-json.a
	install -m 0644 dist/lib/libmbus-json-cJSON.a /usr/local/lib/libmbus-json-cJSON.a
	install -m 0644 dist/lib/libmbus-server.a /usr/local/lib/libmbus-server.a
	install -m 0644 dist/lib/libmbus-socket.a /usr/local/lib/libmbus-socket.a

	install -d /usr/local/lib/pkgconfig
	install -m 0644 libmbus-client.pc /usr/local/lib/pkgconfig/libmbus-client.pc

uninstall:
	rm -f /usr/local/bin/mbus-cli
	rm -f /usr/local/bin/mbus-controller
	rm -f /usr/local/bin/mbus-listener
	
	rm -f /usr/local/include/mbus/client.h
	rm -f /usr/local/include/mbus/debug.h
	rm -f /usr/local/include/mbus/exec.h
	rm -f /usr/local/include/mbus/json.h
	rm -f /usr/local/include/mbus/method.h
	rm -f /usr/local/include/mbus/server.h
	rm -f /usr/local/include/mbus/socket.h
	rm -f /usr/local/include/mbus/tailq.h
	rm -rf /usr/local/include/mbus
	
	rm -f /usr/local/lib/libmbus-client.so
	rm -f /usr/local/lib/libmbus-debug.so
	rm -f /usr/local/lib/libmbus-exec.so
	rm -f /usr/local/lib/libmbus-json.so
	rm -f /usr/local/lib/libmbus-json-cJSON.so
	rm -f /usr/local/lib/libmbus-server.so
	rm -f /usr/local/lib/libmbus-socket.so
	
	rm -f /usr/local/lib/libmbus-client.a
	rm -f /usr/local/lib/libmbus-debug.a
	rm -f /usr/local/lib/libmbus-exec.a
	rm -f /usr/local/lib/libmbus-json.a
	rm -f /usr/local/lib/libmbus-json-cJSON.a
	rm -f /usr/local/lib/libmbus-server.a
	rm -f /usr/local/lib/libmbus-socket.a

	rm -f /usr/local/lib/pkgconfig/libmbus-client.pc
