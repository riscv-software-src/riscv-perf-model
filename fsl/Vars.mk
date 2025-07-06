# Default compiler/flags
CXX = g++
CPP = g++
CC  = gcc

CFLAGS = -Wall -g
CPPFLAGS = -Wall -g

# Doxygen
DOXY=doxygen

# Yacc/Bison 
BISON = bison
#B_CNTR = -Wcounterexamples
BISONFLAGS = -d $(B_CNTR)

# Lex/Flex
FLEX = flex

BOOST_INC= -I/usr/include/boost
BOOST_LIB= -L/usr/lib/x86_64-linux-gnu
