ifeq ($(ROOT),)
	ROOT = $(shell pwd)/../
endif

# The version of the library
include ../Makefile.in

FLAGS = $(DEP_CPPFLAGS) -I$(ROOT)/src

DFLAGS = -D DEBUG -g -Wall -Werror $(FLAGS)
RFLAGS = -D NDEBUG -O3 -Wall -Werror $(FLAGS)

LDFLAGS = $(DEP_LDFLAGS) -L$(ROOT)/src

RPATH := $(DEP_RPATHS) -Wl,-rpath=$(ROOT)/src

LIBS := $(DEP_LIBS) -lprotobuf -l$(PROJECT_NAME)

PROGRAMS = cli srv

cli_obj = echo.pb.o cli.o
srv_obj = echo.pb.o srv.o

release: export CXXFLAGS := $(CXXFLAGS) $(RFLAGS)
debug: export CXXFLAGS := $(CXXFLAGS) $(DFLAGS)

.PHONY: release
release:
	@$(MAKE) $(PROGRAMS) --no-print-directory -s

.PHONY: debug
debug:
	@$(MAKE) $(PROGRAMS) --no-print-directory -s

clean:
	-rm -f $(PROGRAMS) *.o *.d *.pb.h *.pb.cc

srv: $(srv_obj)
	@echo "Linking: $@"
	$(CC) $(LDFLAGS) $(RPATH) $(LIBS) $^ -o $@

cli: $(cli_obj)
	@echo "Linking: $@"
	$(CC) $(LDFLAGS) $(RPATH) $(LIBS) $^ -o $@

%.o: %.cc
	@echo "Compiling: $< -> $@"
	$(CC) $(CXXFLAGS) -MP -MMD -c $< -o $@

%.pb.cc %.pb.h: %.proto
	@echo "Compiling: $< -> $@"
	protoc --cpp_out=./ *.proto
