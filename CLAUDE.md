# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

The SmartMet TimeSeries plugin (`smartmet-plugin-timeseries`) — an HTTP request handler for the SmartMet Server that serves meteorological time series data (forecasts, observations, grid data) via query parameters. It produces `timeseries.so`, loaded dynamically by the server at runtime.

## Build commands

```bash
make                    # Build timeseries.so (release)
make debug              # Debug build
make clean              # Clean build artifacts
make format             # Run clang-format on all source files
make install            # Install to $(plugindir)
make rpm                # Build RPM package
```

## Testing

Tests are **integration tests** — they start a SmartMet Server reactor, send HTTP requests from `test/base/input/*.get` files, and compare responses against `test/base/output/*.get` expected results. The test runner is `smartmet-plugin-test`.

```bash
make test               # Run all tests (sqlite + oracle + postgresql + grid)
make test-sqlite        # SQLite only (fast, no external DB — CI default)
make test-oracle        # Oracle backend tests
make test-postgresql    # PostgreSQL backend tests
make test-grid          # Grid engine tests (requires Redis)
```

Run a subset of tests with `TEST_FILTER`:
```bash
make -C test/base test-sqlite TEST_FILTER="area_t2m"
```

Test timeout defaults to 300 seconds, override with `TEST_TIMEOUT`.

Failed tests write diffs to `test/base/failures-{sqlite,oracle,postgresql}/`. To regenerate the test database: `make -C test/base testdatabase`.

Tests skip certain cases via `.testignore_*` files in `test/base/input/`.

## Architecture

### Request flow

```
HTTP request → Plugin::requestHandler()
  → State (fixes wall-clock time, holds per-query caches)
  → Query (parsed request parameters)
  → QueryProcessingHub::processQuery()
      ├── QEngineQuery     → querydata engine (forecast/model data)
      ├── ObsEngineQuery   → observation engine (station observations)
      └── GridEngineQuery  → grid engine (GRIB/NetCDF grids)
  → PostProcessing (aggregation, precision, formatting)
  → HTTP response
```

### Engine dependencies

The plugin depends on four SmartMet engines (resolved at runtime via dlopen, not at link time):

| Engine | Purpose | Required? |
|--------|---------|-----------|
| `querydata` | Forecast/model data (QueryData format) | Yes |
| `geonames` | Location/geography lookups | Yes |
| `gis` | Geometries, PostGIS integration | Yes |
| `observation` | Station observation data (SQLite/PostgreSQL/Oracle) | Optional (`WITHOUT_OBSERVATION` flag) |
| `grid` | Grid data (GRIB1/GRIB2/NetCDF) | Optional |

Engine references (`SmartMet::Engine::*`) are deliberately unresolved at link time — the build verifies no *other* undefined symbols exist.

### Key source files (all in `timeseries/`)

- **Plugin.cpp** — entry point, lifecycle, HTTP dispatch
- **State.cpp** — per-query context with querydata caches (QCache, TimedQCache)
- **Query.cpp** — HTTP request parameter parsing into structured query
- **QueryProcessingHub.cpp** — routes queries to the appropriate engine processor
- **QEngineQuery.cpp** — forecast data retrieval from querydata engine
- **ObsEngineQuery.cpp** — observation data retrieval (conditionally compiled)
- **GridEngineQuery.cpp** — grid data retrieval, uses QueryStreamer for large results
- **PostProcessing.cpp** — aggregation, precision adjustment, table formatting
- **LocationTools.cpp** — WKT parsing, geometry operations, SVG path generation
- **Config.cpp** — libconfig configuration parsing (precision, producers, formatters)
- **ProducerDataPeriod.cpp** — data availability window tracking per producer

### Configuration

Uses libconfig++ (`.conf` files). Key settings: precision tables, default producers, output formatters, PostGIS identifier mappings, request limits, time series cache config.
