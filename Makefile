
subdir-y = \
	app \
	cjson \
	src \
	test

app_depends-y = \
	src \
	test

src_depends-y = \
	cjson

test_depends-y = \
	src

include Makefile.lib
