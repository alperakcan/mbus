
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
