PROG = PluginTest

REQUIRES = gdal configpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

CFLAGS = -DUNIX -O0 -g $(FLAGS) -Wno-unknown-pragmas

# Unfortunately otlv4.h forces us to define the full oracle include path
INCLUDES += \
	-I ../timeseries \
	-isystem /usr/include/mysql \
	-isystem /usr/include/soci \
	-isystem /usr/include/oracle/11.2/client64

LIBS +=  -lsmartmet-spine  \
	-lsmartmet-macgyver \
	-lsmartmet-newbase \
	$(CONFIGPP_LIBS) \
	-lboost_date_time \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_iostreams \
	-lboost_system \
	-lpthread -ldl

all: $(PROG)
clean:
	rm -f $(PROG) *~

test: $(PROG) test_oracle test_sqlite test_postgresql
	@test `find . -name \*.err | wc -l` = "0" || ( echo ; echo "The following tests have errors:" ; \
	   for i in *.err ; do echo `basename $$i .err`; done ; rm -f *.err ; false )

test_sqlite:
	@echo RUNNING SQLITE TESTS:
	@echo
	@rm -rf failures_sqlite
	@mkdir -p failures
	@rm -f failures/* sqlite.err
	@./PluginTest -c cnf/reactor_sqlite.conf || touch PluginTest.err
	@mv failures failures_sqlite
	@test -f PluginTest.err && mv PluginTest.err sqlite.err || echo All sqlite tests passed
	@echo

test_oracle:
ifndef CI
	@echo RUNNING ORACLE TESTS:
	@echo
	@rm -rf failures_oracle
	@mkdir -p failures
	@rm -f failures/* oracle.err
	@./PluginTest -c cnf/reactor_oracle.conf
	@mv failures failures_oracle
	@test -f PluginTest.err && mv PluginTest.err oracle.err || echo All Oracle tests passed
	@echo
endif

test_postgresql:
ifndef CI
	@echo RUNNING POSTGRESQL TESTS:
	@echo
	@rm -rf failures_postgresql
	@mkdir -p failures
	@rm -f failures/* postgresql.err
	@./PluginTest -c cnf/reactor_postgresql.conf || touch PluginTest.err
	@mv failures failures_postgresql
	@test -f PluginTest.err && mv PluginTest.err postgresql.err || echo All PostGreSQL tests passed
	@echo
endif

$(PROG) : % : %.cpp Makefile ../timeseries.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
