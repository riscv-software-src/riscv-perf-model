
all: small_core.alf medium_core.alf big_core.alf

%_core.alf:
	m4 -DARCH_CORE_COMMON=$*_core_column.cmd core.cmd > $*_core.cmd
	./gen_alf $*_core.cmd  > $*_core.alf

clean:
	rm small_core.cmd medium_core.cmd big_core.cmd *.alf
