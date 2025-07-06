.PHONY: clean default run
include ../../Vars.mk

FSL_INC    = -I../..
MAVIS_INC  = -I../../modules/cpm.mavis
CMN_INC    = -I../common
INTERP_LIB = -L../../fsl_interp/lib -lfslinterp
MAVIS_LIB  = -L../../modules/cpm.mavis/release  -lmavis

ALL_SRC = $(wildcard src/*.cpp)
SRC_OBJ = $(subst src,obj,$(ALL_SRC:.cpp=.o))
ALL_OBJ = $(SRC_OBJ) 
TARGET    = bin/fsttst

INC  = -I./inc $(CMN_INC) $(FSL_INC) $(MAVIS_INC) -I./obj
LIBS = $(INTERP_LIB) $(MAVIS_LIB) -lboost_program_options 

CFLAGS  = $(INC)
LDFLAGS =

default: test

only: $(TARGET)

$(INTERP_LIB):
	$(MAKE) -C ../../fsl_interp lib_only

$(TARGET): $(ALL_OBJ) $(INTERP_LIB)
	@mkdir -p bin
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

obj/%.o : src/%.cpp
	$(CXX) -c $(CFLAGS) $< -o $@

obj/yy.tab.o: $(BISON_OUT)
	$(CXX) -c $(CFLAGS) $< -o $@

obj/lex.yy.o: $(FLEX_OUT)
	$(CXX) -c $(CFLAGS) $< -o $@

# Bison build rule, generating both .c and .h files
$(BISON_OUT) $(BISON_H): $(BISON_SRC)
	@mkdir -p obj
	$(BISON) $(BISONFLAGS) $(BISON_SRC) -o $(BISON_OUT)

# Flex build rule, depends on Bison header
$(FLEX_OUT): $(FLEX_SRC) $(BISON_H)
	@mkdir -p obj
	$(FLEX) -o $(FLEX_OUT) $(FLEX_SRC)

#TRACE=--trace_en
ISA_FILES = --isa_file ./json/isa_rv64g.json \
            --isa_file ./json/isa_rv64c.json

FSL_SYNTAX_FILES = --fsl_syntax_file ./fsl/test1.fsl

test:  $(TARGET)
	$(TARGET) --tb_verbose \
      $(ISA_FILES) $(FSL_FILES) $(FSL_SYNTAX_FILES) $(TRACE)

help-%:
	@echo $* = $($*)

clean:
	rm -f ./bin/* ./obj/*


