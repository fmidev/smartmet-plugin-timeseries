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

ifdef CI
test: $(PROG) fminames test_sqlite
else
test: $(PROG) test_sqlite test_oracle test_postgresql
endif
	@test `find . -name \*.err | wc -l` = "0" || ( echo ; echo "The following tests have errors:" ; \
	   for i in *.err ; do echo `basename $$i .err`; done ; rm -f *.err ; false )

fminames:
	sed -i -e 's/"smartmet-test"/"localhost"/g' cnf/*.conf
	@/usr/share/smartmet/test/db/init-and-start.sh && /usr/share/smartmet/test/db/install-test-db.sh > /dev/null

test_sqlite: $(PROG)
	@printf "RUNNING SQLITE TESTS:\n\n"
	@rm -rf failures_sqlite failures sqlite.err input/.testignore
	@mkdir -p failures
	@test -f input/.testignore_sqlite && cp input/.testignore_sqlite input/.testignore || true
	@./PluginTest -c cnf/reactor_sqlite.conf
	@mv failures failures_sqlite
	@test -f PluginTest.err && mv PluginTest.err sqlite.err || echo All sqlite tests passed
	@printf "\n"

test_oracle: $(PROG)
	@printf "RUNNING ORACLE TESTS:\n\n"
	@rm -rf failures_oracle failures oracle.err input/.testignore
	@mkdir -p failures
	@test -f input/.testignore_oracle && cp input/.testignore_oracle input/.testignore || true
	@./PluginTest -c cnf/reactor_oracle.conf
	@rm -f input/.testignore
	@mv failures failures_oracle
	@test -f PluginTest.err && mv PluginTest.err oracle.err || echo All Oracle tests passed
	@printf "\n";

test_postgresql: $(PROG)
	@printf "RUNNING POSTGRESQL TESTS:\n\n"
	@rm -rf failures_postgresql failures postgresql.err input/.testignore
	@mkdir -p failures
	@test -f input/.testignore_postgresql && cp input/.testignore_postgresql input/.testignore || true
	@./PluginTest -c cnf/reactor_postgresql.conf
	@rm -f input/.testignore
	@mv failures failures_postgresql
	@test -f PluginTest.err && mv PluginTest.err postgresql.err || echo All PostGreSQL tests passed
	@printf "\n";

testdatabase: $(PROG)
	@printf "CREATING TEST DATABASE:\n\n"
	@ls -1 input > input/.testignore
	@./PluginTest -c cnf/reactor_testdb.conf && echo .quit | spatialite -silent test_cache.sqlite
	@rm -f input/.testignore

$(PROG) : % : %.cpp Makefile ../timeseries.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
