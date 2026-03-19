CXXFLAGS := -std=c++17 -Wall $(if $(DEBUG),-ggdb,-O3) -I../model -I../../lib/quantor-3.2
CXX      := g++
CC       := g++

LIBDIR   := ../../lib
LIBS     := ../model/libqbm.a $(LIBDIR)/libquantor.a $(LIBDIR)/libipasir_dummy.so
LDFLAGS  := -L../model -L$(LIBDIR) -Wl,-rpath,'$$ORIGIN/../lib'

OBJECTS  := qdlsolve.o QdlParser.o

.PHONY: all clean clobber FORCE

## Standard Targets ##########################################################
all: qdlsolve

clean:
	rm -rf *~ *.o

# After clobber the QDLParser must be re-built using wisent!
clobber: clean
	rm -rf QdlParser.cpp QdlParser.hpp

## Individual Executables ####################################################
qdlsolve: LDLIBS := -lqbm -lquantor -lipasir_dummy
qdlsolve: qdlsolve.o QdlParser.o

## Dependencies ##############################################################

# Build Parser
QdlParser.cpp QdlParser.hpp: QdlParser.ypp
	wisent $<

# Force Visit of External Libraries
$(OBJECTS): $(LIBS)
$(LIBS): FORCE
	$(MAKE) -C $(dir $@) $(notdir $@)

