
all: small_core.alf medium_core.alf big_core.alf

%_core.alf:
	@echo "Generating pipeout for $@"
	@(cd ../release; ./olympia -z ../layouts/pipeout -i1 traces/dhry_riscv.zstf > /dev/null)
	@./gen_alf.py -d pipeoutlocation.dat -a $*_core.alf
	@rm pipeout*

clean:
	rm -f *.alf pipeout*
