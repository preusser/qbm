CXXFLAGS := -std=c++17 -Wall $(if $(DEBUG),-ggdb,-O3) -I../../lib/quantor-3.2
CXX	 := g++
CC	 := g++

LIBDIR   := ../../lib
LIBS     := $(LIBDIR)/libquantor.a $(LIBDIR)/libipasir_dummy.so
LDFLAGS  := -L$(LIBDIR) -Wl,-rpath,'$$ORIGIN/../../lib'

OBJECTS  := CompDecl.o Statement.o PortDecl.o WireDecl.o \
	    Expression.o ParamDecl.o Lib.o Root.o Context.o Result.o Scope.o

.PHONY: default all clean clobber FORCE

## Standard Targets ##########################################################
default: libqbm.a
all: default test

clean:
	rm -rf *~ lib*.a *.o test

clobber: clean

## Actual Outputs ############################################################
libqbm.a: $(OBJECTS)
	$(AR) rcs $@ $^

$(OBJECTS) test.o: $(LIBS)

test: LDLIBS := -L. -lqbm -lquantor -lipasir_dummy
test: test.o

## Dependencies ##############################################################

# Force Visit of External Libraries
$(OBJECTS): $(LIBS)
$(LIBS): FORCE
	$(MAKE) -C $(dir $@) $(notdir $@)
