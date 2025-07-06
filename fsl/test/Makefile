.PHONY: default regress test test_interp test_api

include ../Vars.mk

default: regress
test: regress
regress: test_interp test_api

test_interp:
	$(MAKE) -C interp test

test_api:
	$(MAKE) -C api test

clean:
	$(MAKE) -C interp clean
	$(MAKE) -C api clean
