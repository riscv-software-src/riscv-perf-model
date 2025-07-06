.PHONY: interp api fusion \
          test \
          regress \
        docs \
          docs_interp \
          docs_api \
        help \
        clean

MAVIS_LIB = modules/cpm.mavis/release/libmavis.a

default: $(MAVIS_LIB) interp api test docs

$(MAVIS_LIB):
	cd modules/cpm.mavis; \
	mkdir -p release; \
	cd release; \
	cmake .. -DCMAKE_BUILD_TYPE=Release; \
	make -j32;
	
interp: $(MAVIS_LIB)
	$(MAKE) -C fsl_interp only

api: $(MAVIS_LIB)
	# Header only. Nothing to do at the moment
	#	$(MAKE) -C fsl_api only

test: $(MAVIS_LIB) regress

regress: 
	$(MAKE) -C ./test regress

docs: docs_interp docs_api

docs_interp:
	$(MAKE) -C ./docs docs_interp

docs_api:
	$(MAKE) -C ./docs docs_api

help:
	@echo "-E: Implement make help"

clean:
	$(MAKE) -C fsl_interp clean
	$(MAKE) -C fsl_api clean
	$(MAKE) -C test clean
	$(MAKE) -C docs clean
