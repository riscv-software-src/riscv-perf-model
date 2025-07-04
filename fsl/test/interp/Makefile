.PHONY: clean default run
include ../../Vars.mk
#
INP_FILES = -i syntax_tests/_1_syntax_test.fsl \
            -i syntax_tests/_2_syntax_test.fsl \
            -i syntax_tests/sample1.fsl \
            -i syntax_tests/all_comments.fsl \
            -i syntax_tests/empty_file.fsl \
            -i syntax_tests/syntax1.fsl \
            -i syntax_tests/syntax2.fsl \
            -i syntax_tests/test3.fsl \
            -i syntax_tests/test4.fsl \

INTERP_BIN=../../fsl_interp/bin/fslinterp

OUT_FILE  = -o output.txt

default: $(INTERP_BIN)
	$(MAKE) test

$(INTERP_BIN):
	$(MAKE) -C ../../fsl_interp interp_only

#TRACE=--trace_en
test:  $(INTERP_BIN)
	$(INTERP_BIN) --verbose $(INP_FILES) $(OUT_FILE) $(TRACE)

help-%:
	@echo $* = $($*)

# Clean build files
clean:
	rm -f ./bin/* ./obj/*


