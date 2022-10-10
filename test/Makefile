
include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

TEST_PREPARE_TARGETS := cnf/geonames.conf cnf/gis.conf
TEST_FINISH_TARGETS := dummy

TEST_DB_DIR := $(shell pwd)/tmp-geonames-db

ifdef CI
GEONAMES_HOST_EDIT := sed -e 's|"smartmet-test"|"$(TEST_DB_DIR)"|g'
TEST_PREPARE_TARGETS += start-geonames-db
TEST_FINISH_TARGETS += stop-test-db
TEST_TARGETS := test-sqlite
EXTRA_IGNORE := input/.testignore_circle-ci
else
GEONAMES_HOST_EDIT := cat
TEST_TARGETS := test-sqlite test-oracle test-postgresql
EXTRA_IGNORE :=
endif

TEST_RUNNER :=

all:

clean:
	-$(MAKE) $(TEST_FINISH_TARGETS)
	rm -rf failed-sqlite failed-oracle failed-postgresql
	rm -rf tmp-geonames-db
#	Remove following line later
	rm -rf conf/grid

test:	$(TEST_PREPARE_TARGETS)
	ok=true; \
	if $(MAKE) -C base $@; then ok=true; else ok=false; fi; \
	if ! $(MAKE) -C grid test; then ok=false; fi; \
	$(MAKE) $(TEST_FINISH_TARGETS); \
	$$ok

test-sqlite test-oracle test-postgresql: $(TEST_PREPARE_TARGETS)
	ok=true
	if $(MAKE) -C base $@; then ok=true; else ok=false; fi; \
	$(MAKE) $(TEST_FINISH_TARGETS); \
	$$ok

test-grid:	$(TEST_PREPARE_TARGETS)
	ok=true
	if $(MAKE) -C grid test; then ok=true; else ok=false; fi; \
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
