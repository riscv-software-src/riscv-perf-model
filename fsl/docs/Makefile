.PHONY: default  \
        docs \
          docs_interp \
          docs_api \
        clean

include ../Vars.mk

default: docs

docs: docs_interp docs_api

docs_interp:
	-mkdir -p docs_interp
	$(DOXY) DoxyInterp
	
docs_api:
	-mkdir -p docs_api
	$(DOXY) DoxyApi

clean:
	-rm -rf interp fusion api docs_interp docs_api
	-rm -f *_warn.log
