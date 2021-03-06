TEST_DB_DIR := $(shell pwd)/../tmp-geonames-db

test-oracle:		DB_TYPE := oracle
test-postgresql:	DB_TYPE := postgresql
test-sqlite:		DB_TYPE := sqlite

ifdef CI
TEST_TARGETS := test-sqlite
EXTRA_IGNORE := input/.testignore_ci
else
TEST_TARGETS := test-sqlite test-oracle test-postgresql
EXTRA_IGNORE :=
endif

test-sqlite test-oracle test-postgresql:
	rm -rf failures-$(DB_TYPE)
	mkdir -p failures-$(DB_TYPE) tmp
	ok=true; $(TEST_RUNNER) smartmet-plugin-test --handler /timeseries \
		--reactor-config cnf/reactor_$(DB_TYPE).conf \
		--failures-dir failures-$(DB_TYPE) \
		$(foreach fn, input/.testignore_$(DB_TYPE) $(EXTRA_IGNORE), --ignore $(fn)) \
		--timeout 300 || ok=false; \
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

all:

clean:
	rm -rf failed-sqlite failed-oracle failed-postgresql

test:
	@ok=true; failed=; \
	@mkdir -p tmp; \
	for test in $(TEST_TARGETS); do \
	  if ! $(MAKE) $$test; then ok=false; failed="$$failed $$test"; fi; \
        done; \
	if ! $$ok ; then \
	  echo "======================================================================="; \
	  echo "Failed: $$failed"; \
	  echo "======================================================================="; \
        fi; \
        $$ok

.PHONY: test test-sqlite test-oracle test-sqlite test-database
