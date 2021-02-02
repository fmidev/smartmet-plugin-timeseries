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

TEST_PREPARE_TARGETS := cnf/geonames.conf cnf/gis.conf
TEST_FINISH_TARGETS := dummy


ifdef CI
GEONAMES_HOST_EDIT := sed -e 's/"smartmet-test"/"localhost"/g'
TEST_PREPARE_TARGETS += start-geonames-db
TEST_FINISH_TARGETS += stop-test-db
TEST_TARGETS := test-sqlite
EXTRA_IGNORE := input/.testignore_circle-ci
else
GEONAMES_HOST_EDIT := cat
TEST_TARGETS := test-sqlite test-oracle test-postgresql
EXTRA_IGNORE :=
endif

TEST_DB_DIR := $(shell pwd)/tmp-geonames-db
TEST_RUNNER :=

all:

clean:
	-$(MAKE) $(TEST_FINISH_TARGETS)
	rm -rf failed-sqlite failed-oracle failed-postgresql
	rm -rf tmp-geonames-db

test:
	@ok=true; failed=; \
	for test in $(TEST_TARGETS); do \
	  if ! $(MAKE) $$test; then ok=false; failed="$$failed $$test"; fi; \
        done; \
	if ! $$ok ; then \
	  echo "======================================================================="; \
	  echo "Failed: $$failed"; \
	  echo "======================================================================="; \
        fi; \
        $$ok

PRE_TEST :=
POST_TEST :=

test-oracle:		DB_TYPE := oracle
test-postgresql:	DB_TYPE := postgresql
test-sqlite:		DB_TYPE := sqlite

test-sqlite test-oracle test-postgresql: $(TEST_PREPARE_TARGETS)
	rm -rf failures-$(DB_TYPE)
	mkdir -p failures-$(DB_TYPE)
	ok=true; $(TEST_RUNNER) smartmet-plugin-test --handler /timeseries \
		--reactor-config cnf/reactor_$(DB_TYPE).conf \
		--failures-dir failures-$(DB_TYPE) \
		$(foreach fn, input/.testignore_$(DB_TYPE) $(EXTRA_IGNORE), --ignore $(fn)) \
		--timeout 300 || ok=false; \
	$(MAKE) $(TEST_FINISH_TARGETS); \
	$$ok

testdatabase:
	@printf "CREATING TEST DATABASE:\n\n"
	@ls -1 input > input/.testignore
	@rm -f test_cache.sqlite
	@$(TEST_RUNNER) smartmet-plugin-test --handler /timeseries \
		--reactor-config cnf/reactor_testdb.conf \
		--ignore input/.testignore \
		&& \
	echo .quit | spatialite -silent test_cache.sqlite
	@rm -f input/.testignore

geonames-database:
	@-$(MAKE) stop-test-db
	rm -rf tmp-geonames-db
	if ! /usr/share/smartmet/test/db/create-local-db.sh $(TEST_DB_DIR) >tmp-geonames-db.log 2>&1 ; then \
	    cat tmp-geonames-db.log; \
	    false; \
	fi

start-geonames-db: geonames-database
	/usr/share/smartmet/test/db/test-db-ctl.sh $(TEST_DB_DIR) start -w

stop-test-db:
	-/usr/share/smartmet/test/db/test-db-ctl.sh $(TEST_DB_DIR) stop

cnf/geonames.conf cnf/gis.conf:
	$(GEONAMES_HOST_EDIT) $@.in >$@

dummy:

.PHONY: cnf/geonames.conf cnf/gis.conf
