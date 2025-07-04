.PHONY: clean default lib_only interp_only

include ../Vars.mk

ALL_SRC = $(wildcard src/*.cpp)
SRC_OBJ = $(subst src,obj,$(ALL_SRC:.cpp=.o))
LIB_OBJ = obj/FslParser.o obj/lex.yy.o obj/Options.o obj/yy.tab.o

# Files
BISON_SRC     = src/fsl.y
FLEX_SRC      = src/fsl.l
BISON_OUT     = obj/yy.tab.cpp
BISON_H       = obj/yy.tab.hpp
FLEX_OUT      = obj/lex.yy.cpp
INTERP_TARGET = bin/fslinterp
LIB_TARGET    = lib/libfslinterp.a

PARSE_OBJ = $(BISON_OUT:.cpp=.o) $(FLEX_OUT:.cpp=.o)

ALL_OBJ = $(SRC_OBJ) $(PARSE_OBJ)

INC  = -I./inc -I./obj $(BOOST_INC)
LIBS = $(BOOST_LIB) -lboost_program_options

INP_FILES = -i syntax_tests/_1_syntax_test.fsl \
            -i syntax_tests/_2_syntax_test.fsl \
            -i syntax_tests/sample1.fsl

OUT_FILE  = -o output.txt

CFLAGS  = $(INC)
LDFLAGS =

default: lib_only interp_only
only: lib_only interp_only

lib_only:    $(LIB_TARGET)
interp_only: $(INTERP_TARGET)

$(INTERP_TARGET): $(ALL_OBJ)
	@mkdir -p bin
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

$(LIB_TARGET): $(LIB_OBJ)
	@mkdir -p lib
	ar rcs $(LIB_TARGET) $^

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

##TRACE=--trace_en
#run:  $(TARGET)
#	$(TARGET) --verbose $(INP_FILES) $(OUT_FILE) $(TRACE)

help-%:
	@echo $* = $($*)

# Clean build files
clean:
	rm -f ./bin/* ./obj/*


