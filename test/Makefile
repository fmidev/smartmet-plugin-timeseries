
include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

TEST_PREPARE_TARGETS := cnf/geonames.conf cnf/gis.conf
TEST_FINISH_TARGETS := dummy

TEST_DB_DIR := $(shell pwd)/tmp-geonames-db

ifdef CI
GEONAMES_HOST_EDIT := sed -e 's|"smartmet-test"|"$(TEST_DB_DIR)"|g'
TEST_PREPARE_TARGETS += start-geonames-db start-redis-db
TEST_FINISH_TARGETS += stop-test-db stop-redis-db
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
	rm -rf conf/grid

test:	$(TEST_PREPARE_TARGETS)
	ok=true; \
	if $(MAKE) -C base $@; then ok=true; else ok=false; fi; \
	if ! $(MAKE) -C grid test; then ok=false; fi; \
	-$(MAKE) $(TEST_FINISH_TARGETS); \
	$$ok

test-sqlite test-oracle test-posgresql: $(TEST_PREPARE_TARGETS)
	ok=true
	if $(MAKE) -C base $@; then ok=true; else ok=false; fi; \
	-$(MAKE) $(TEST_FINISH_TARGETS); \
	$$ok

test-grid:	$(TEST_PREPARE_TARGETS)
	ok=true
	if $(MAKE) -C grid test; then ok=true; else ok=false; fi; \
	-$(MAKE) $(TEST_FINISH_TARGETS); \
	$$ok

testdatabase:
	@printf "CREATING TEST DATABASE:\n\n"

PRE_TEST :=
POST_TEST :=

test-oracle:		DB_TYPE := oracle
test-postgresql:	DB_TYPE := postgresql
test-sqlite:		DB_TYPE := sqlite

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

start-redis-db:
	rm -rf cnf/grid
	cp -pr $(datadir)/smartmet/test/grid cnf/
	@echo "*** Creating the Redis configuration file."
	smartmet-grid-test-config-creator cnf/environment.conf cnf/grid/redis/redis-template.conf cnf/grid/redis/redis.conf
	@echo "*** Starting the Redis database."
	@redis-server cnf/grid/redis/redis.conf

stop-redis-db:
	@if [ -f cnf/grid/redis/redis-server.pid ] ; then \
		cat cnf/grid/redis/redis-server.pid | xargs kill -9; \
		rm -f cnf/grid/redis/redis-server.pid; \
	fi

cnf/geonames.conf cnf/gis.conf:
	$(GEONAMES_HOST_EDIT) $@.in >$@

dummy:

.PHONY: cnf/geonames.conf cnf/gis.conf
