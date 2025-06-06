TEST_DB_DIR := $(shell pwd)/../tmp-geonames-db

test-oracle:		DB_TYPE := oracle
test-postgresql:	DB_TYPE := postgresql
test-sqlite:		DB_TYPE := sqlite

TEST_TIMEOUT ?= 300

ifdef CI
TEST_TARGETS := test-sqlite
EXTRA_IGNORE := input/.testignore_ci
else
TEST_TARGETS := test-sqlite test-oracle test-postgresql
EXTRA_IGNORE :=
endif

ifdef TEST_FILTER
SMARTMET_PLUGIN_TEST_FILTER := --filter "$(TEST_FILTER)"
else
SMARTMET_PLUGIN_TEST_FILTER :=
endif

test-sqlite test-oracle test-postgresql:
	rm -rf failures-$(DB_TYPE)
	rm -rf tmp && mkdir tmp && cp output/* tmp/
	mkdir -p failures-$(DB_TYPE)
	ok=true; $(TEST_RUNNER) smartmet-plugin-test --handler /timeseries -e output/ \
		--reactor-config cnf/reactor_$(DB_TYPE).conf \
		$(SMARTMET_PLUGIN_TEST_FILTER) \
		--failures-dir failures-$(DB_TYPE) \
		$(foreach fn, input/.testignore_$(DB_TYPE) $(EXTRA_IGNORE), --ignore $(fn)) \
		--timeout $(TEST_TIMEOUT) || ok=false; \
	$$ok

testdatabase:
	@printf "CREATING TEST DATABASE:\n\n"
	@ls -1 input > input/.testignore
	@rm -f test_cache.sqlite
	@spatialite -silent test_cache.sqlite VACUUM
	@$(TEST_RUNNER) smartmet-plugin-test --handler /timeseries \
		--reactor-config cnf/reactor_testdb.conf \
		--ignore input/.testignore
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
