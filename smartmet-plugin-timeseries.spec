%bcond_without observation
%define DIRNAME timeseries
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet timeseries plugin
Name: %{SPECNAME}
Version: 24.5.24
Release: 1%{?dist}.fmi
License: MIT
Group: SmartMet/Plugins
URL: https://github.com/fmidev/smartmet-plugin-timeseries
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%if 0%{?rhel} && 0%{rhel} < 9
%define smartmet_boost boost169
%else
%define smartmet_boost boost
%endif

%define smartmet_fmt_min 8.1.1
%define smartmet_fmt_max 8.2.0

BuildRequires: rpm-build
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: %{smartmet_boost}-devel
BuildRequires: fmt-devel >= %{smartmet_fmt_min}, fmt-devel < %{smartmet_fmt_max}
BuildRequires: bzip2-devel
BuildRequires: zlib-devel
BuildRequires: smartmet-library-timeseries-devel >= 24.5.21
BuildRequires: smartmet-library-spine-devel >= 24.5.21
BuildRequires: smartmet-library-locus-devel >= 23.7.28
BuildRequires: smartmet-library-macgyver-devel >= 24.5.16
BuildRequires: smartmet-library-grid-content-devel >= 24.5.8
BuildRequires: smartmet-library-grid-files-devel >= 24.5.22
BuildRequires: smartmet-library-newbase-devel >= 24.5.17
BuildRequires: smartmet-library-gis-devel >= 24.4.24
BuildRequires: smartmet-engine-geonames-devel >= 24.5.16
%if %{with observation}
BuildRequires: smartmet-engine-observation-devel >= 24.5.16
%endif
BuildRequires: smartmet-engine-querydata-devel >= 24.5.16
BuildRequires: smartmet-engine-gis-devel >= 24.5.16
BuildRequires: smartmet-engine-grid-devel >= 24.5.16
# obsengine can be disabled in configuration: not included intentionally
#%if %{with observation}
#Requires: smartmet-engine-observation >= 24.5.16
#%endif
Requires: fmt >= %{smartmet_fmt_min}, fmt < %{smartmet_fmt_max}
Requires: smartmet-library-gis >= 24.4.24
Requires: smartmet-library-locus >= 23.7.28
Requires: smartmet-library-macgyver >= 24.5.16
Requires: smartmet-library-newbase >= 24.5.17
Requires: smartmet-library-spine >= 24.5.21
Requires: smartmet-library-timeseries >= 24.5.21
Requires: smartmet-engine-geonames >= 24.5.16
Requires: smartmet-engine-querydata >= 24.5.16
Requires: smartmet-engine-gis >= 24.5.16
Requires: smartmet-engine-grid >= 24.5.16
Requires: smartmet-server >= 24.5.16
Requires: %{smartmet_boost}-filesystem
Requires: %{smartmet_boost}-iostreams
Requires: %{smartmet_boost}-system
Requires: %{smartmet_boost}-thread
Provides: %{SPECNAME}
Obsoletes: smartmet-brainstorm-timeseries < 16.11.1
Obsoletes: smartmet-brainstorm-timeseries-debuginfo < 16.11.1
#TestRequires: smartmet-utils-devel >= 24.5.10
#TestRequires: smartmet-library-spine-plugin-test >= 24.5.21
#TestRequires: smartmet-library-newbase-devel >= 24.5.17
#TestRequires: redis
#TestRequires: smartmet-test-db >= 24.5.14
#TestRequires: smartmet-test-data >= 24.5.22
#TestRequires: smartmet-engine-grid-test >= 24.5.16
#TestRequires: smartmet-library-gis >= 24.4.24
#TestRequires: smartmet-engine-geonames >= 24.5.16
#TestRequires: smartmet-engine-gis >= 24.5.16
#TestRequires: smartmet-engine-querydata >= 24.5.16
%if %{with observation}
#TestRequires: smartmet-engine-observation >= 24.5.16
%endif
#TestRequires: smartmet-engine-grid >= 24.5.16
#TestRequires: gdal35
#TestRequires: libwebp13

%description
SmartMet timeseries plugin

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{SPECNAME}
 
%build -q -n %{SPECNAME}
make %{_smp_mflags} \
     %{?!with_observation:CFLAGS=-DWITHOUT_OBSERVATION}

%install
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0775,root,root,0775)
%{_datadir}/smartmet/plugins/timeseries.so

%changelog
* Fri May 24 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.5.24-1.fmi
- WSI improvements

* Wed May 22 2024 Andris Pavenis <andris.pavenis@fmi.fi> 24.5.22-1.fmi
- Fix 2 if statements in ObsEngineQuery

* Thu May 16 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.16-1.fmi
- Clean up boost date-time uses

* Tue May  7 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.7-1.fmi
- Use Date library (https://github.com/HowardHinnant/date) instead of boost date_time

* Fri May  3 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.5.3-1.fmi
- Repackaged due to GRID library changes

* Fri Apr  5 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.4.5-1.fmi
- Added support for WIGOS wsi

* Wed Apr  3 2024 Mika Heiskanen <mheiskan@rhel8.dev.fmi.fi> - 24.4.3-2.fmi
- Fixed potential dereference of an empty boost::optional

* Wed Apr  3 2024 Mika Heiskanen <mheiskan@rhel8.dev.fmi.fi> - 24.4.3-1.fmi
- Fixed start time to the next valid timestep (was previous)

* Fri Mar 22 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.3.22-1.fmi
- Avoid string use for time value manipulations in GridInterface::prepareQueryTimes

* Fri Feb 23 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.2.23-1.fmi
- Full repackaging

* Thu Feb 22 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.2.22-1.fmi
- Repackaged due to ABI changes

* Tue Feb 20 2024 Mika Heiskanen <mheiskan@rhel8.dev.fmi.fi> - 24.2.20-2.fmi
- Repackaged due to grid-files ABI changes

* Tue Feb 20 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.2.20-1.fmi
- Removed support for BK_HYDROMETA data (BRAINSTORM-2867)

* Mon Feb  5 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.2.5-1.fmi
- Repackaged due to grid-files ABI changes

* Tue Jan 30 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.1.30-1.fmi
- Repackaged due to newbase ABI changes

* Thu Jan  4 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.1.4-1.fmi
- Added option 'day' for timeseries generation (BRAINSTORM-2826)

* Fri Dec 22 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.22-1.fmi
- Repackaged due to ThreadLock ABI changes

* Tue Dec  5 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.5-1.fmi
- Repackaged due to an ABI change in SmartMetPlugin

* Mon Dec  4 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.4-1.fmi
- Repackaged due to QEngine ABI changes

* Fri Dec  1 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.1-1.fmi
- Fixed handling of WKT input to properly consider each type of WKT (not just polygons)

* Fri Nov 17 2023 Pertti Kinnia <pertti.kinnia@fmi.fi> - 23.11.17-1.fmi
- Repackaged due to API changes in grid-files and grid-content

* Thu Nov 16 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.11.16-1.fmi
- Added maxradius limit setting

* Fri Nov 10 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.11.10-1.fmi
- Fixed queries where timestep, starttime or endtime was 'data'

* Tue Oct 31 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.31-1.fmi
- Fix to DST change handling

* Mon Oct 30 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.10.30-1.fmi
- Added handling of stationtype parameter (BRAINSTORM-2756)

* Fri Oct 20 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.10.20-1.fmi
- Added tests for amean_t function (BRAINSTORM-2575)

* Thu Oct 12 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.10.12-1.fmi
- Grid supportrelated updates and fixes
- QEngineQuery: do not try to load data levels for pressures and heights

* Wed Oct 11 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.11-2.fmi
- Do not resolve host names for exceptions for normal requests

* Wed Oct 11 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.10.11-1.fmi
- Moved some aggregation related functions to timeseries-library (BRAINSTORM-2457)

* Fri Sep 29 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.9.29-1.fmi
- Refactored code

* Wed Sep 20 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.9.20-1.fmi
- Removed redundant files

* Mon Sep 18 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.9.18-1.fmi
- Refactored code

* Tue Sep 12 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.9.12-1.fmi
- Refactored the code (BRAINSTORM-2694)
- Fixed groupareas=0 bug (BRAINSTORM-2716)

* Mon Sep 11 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.9.11-1.fmi
- Fixed error in determining GRIB data start time

* Mon Jul 31 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.31-1.fmi
- Moved ImageFormatter to Spine

* Fri Jul 28 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.28-2.fmi
- Fixed build after ssmartmet-library-spine changes

* Fri Jul 28 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.28-1.fmi
- Repackage due to bulk ABI changes in macgyver/newbase/spine

* Wed Jul 12 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.12-2.fmi
- Use postgresql 15, gdal 3.5, geos 3.11 and proj-9.0

* Wed Jul 12 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.12-1.fmi
- Use postgresql 15, gdal 3.5, geos 3.11 and proj-9.0

* Tue Jul 11 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.11-1.fmi
- Silenced compiler warnings

* Tue Jun 13 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.13-1.fmi
- Support internal and environment variables in configuration files

* Tue Jun  6 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.6-1.fmi
- Repackaged due to GRID ABI changes

* Thu Jun  1 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.1-1.fmi
- Added stationgroups option

* Wed May 10 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.5.10-1.fmi
- Use latest obs engine API for looking for the latest observations

* Mon Apr 17 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.4.17-1.fmi
- Repackaged due to GRID ABI changes

* Tue Mar 21 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.3.21-1.fmi
- Disable stack trace & logging of queries without location options

* Thu Mar 16 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.3.16-3.fmi
- Disable printing stack traces to journals on trivial user input errors

* Thu Mar 16 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.3.16-2.fmi
- Fixed grid producer detection

* Thu Mar 16 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.3.16-1.fmi
- Silenced compiler warnings

* Wed Mar 15 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.3.15-1.fmi
- Repackaged since timeseries library Stat object ABI changed

* Wed Feb 22 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.2.22-1.fmi
- Fixed handling of missing values for location parameters (BRAINSTORM-2544)

* Mon Feb 20 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.2.20-1.fmi
- Fixed timestep handling bug in observation query(BRAINSTORM-2545)

* Wed Feb  8 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.2.8-1.fmi
- Add host name to stack traces

* Wed Feb 1 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.2.1-1.fmi
- Added language support for station names (BRAINSTORM-2514)

* Thu Jan 26 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.1.26-1.fmi
- Added support for request size limits (BRAINSTORM-2443)

* Thu Jan 19 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.1.19-1.fmi
- Repackaged due to ABI changes in grid libraries

* Wed Jan 11 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.1.11-1.fmi
- Added support for moving stations icebuoy and copernicus (BRAINSTORM-2409)

* Mon Dec 12 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.12-1.fmi
- Repackaged due to ABI changes

* Thu Dec 8 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.12.8-1.fmi
- Updated Fmi IoT tests (BRAINSTORM-2494)
- Preload checking removed as obsolete

* Fri Dec  2 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.12.2-1.fmi
- Update HTTP request method checking and support OPTIONS method

* Fri Nov 25 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.25-1.fmi
- Add apikey to stack traces

* Tue Nov  8 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.8-1.fmi
- Repackaged due to base library ABI changes

* Thu Oct 20 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.20-1.fmi
- Repackaged due to base library ABI changes

* Wed Oct 12 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.12-1.fmi
- Repackaged to require a newer timeseries library

* Mon Oct 10 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.10.10-1.fmi
- Handle special parameters (time and location) in timeseries plugin (BRAINSTORM-2420)

* Wed Oct  5 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.5-1.fmi
- Do not use boost::noncopyable

* Fri Sep  9 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.9-1.fmi
- Fixed several compiler warnings, some of which were on unnecessary copies

* Thu Sep  8 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.8-1.fmi
- Fixes to grib coordinate latlon coordinate handling

* Thu Aug 25 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.25-1.fmi
- Use a generic exception handler for configuration errors

* Wed Aug 17 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.8.17-1.fmi
- Fixed wkt+radius bug (BRAINSTORM-2384)

* Thu Jul 28 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.7.28-1.fmi
- Repackaged due to QEngine ABI change

* Wed Jul 27 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.7.27-1.fmi
- Repackaged since macgyver CacheStats ABI changed

* Tue Jun 21 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.6.21-1.fmi
- Add support for RHEL9, upgrade libpqxx to 7.7.0 (rhel8+) and fmt to 8.1.1

* Mon Jun  6 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.6.6-1.fmi
- Fixed handling of very long parameter names

* Tue May 31 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.5.31-1.fmi
- Repackage due to smartmet-engine-querydata and smartmet-engine-observation ABI changes

* Tue May 24 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.24-1.fmi
- Repackaged due to NFmiArea ABI changes

* Fri May 20 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.20-1.fmi
- Repackaged due to ABI changes to newbase LatLon methods

* Wed May 18 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.18-1.fmi
- Fixed handling of station number parameters, they had no effect on ETag calculations

* Mon May  9 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.5.9-1.fmi
- Tests updated (BRAINSTORM-2297)

* Thu Apr 28 2022 Andris Pavenis <andris.pavenis@fmi.fi> 22.4.28-1.fmi
- Repackage due to SmartMet::Spine::Reactor ABI changes

* Mon Apr 4 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.4.4-1.fmi
- Added magnetometer observation test (BRAINSTORM-2279)

* Mon Mar 28 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.3.28-1.fmi
- Fixed handling of origintime=latest/newest/oldest requests

* Mon Mar 21 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.3.21-2.fmi
- Fixed line positioning for additional query information

* Mon Mar 21 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.3.21-1.fmi
- Update due to changes in smartmet-library-spine and smartnet-library-timeseries

* Wed Mar 16 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.3.16-1.fmi
- Read langauge option into parameter before using it (BRAINSTORM-2271)

* Tue Mar 15 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.3.15-1.fmi
- Fixed overlapping observation/forecast time series when using GRIB data

* Thu Mar 10 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.3.10-1.fmi
- Repackaged due to base library ABI changes

* Tue Mar 8 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.3.8-1.fmi
- Started using timeseries-library (BRAINSTORM-2259)

* Mon Mar  7 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.3.7-1.fmi
- Repackaged due to base library API changes

* Wed Mar  2 2022 Pertti Kinnia <pertti.kinnia@fmi.fi> - 22.3.2-1.fmi
- Bug fix, repackaged (BRAINSTORM-2268)

* Mon Feb 28 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.2.28-1.fmi
- Repackaged due to base library/engine ABI changes

* Wed Feb  9 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.2.9-2.fmi
- Use Fmi::to_string to avoid locale dependent formatting of JSON code

* Wed Feb  9 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.2.9-1.fmi
- Added concatenation of results from different grid producers

* Tue Feb 1 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.2.1-1.fmi
- Test observation engine configuration file updated to contain default number for temperature-parameter (BRAINSTORM-2243)

* Tue Jan 25 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.1.25-1.fmi
- Added possibility to request data from unfinished generations by using analysistime 'any'

* Fri Jan 21 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.1.21-1.fmi
- Repackage due to upgrade of packages from PGDG repo: gdal-3.4, geos-3.10, proj-8.2

* Tue Jan 18 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.1.18-1.fmi
- Use DistanceParser for maxdistance URL-parameter (BRAINSTORM-605)

* Tue Dec  7 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.12.7-1.fmi
- Upgrade to PostgreSQL 13 and GDAL-3.3

* Tue Nov 23 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.11.23-1.fmi
- Support for coordinate transformations by using x- and y-parameters (BRAINSTORM-2091)

* Mon Nov 15 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.11.15-1.fmi
- Repackaged due to ABI changes in base grid libraries

* Wed Nov 10 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.11.10-1.fmi
- Use double-conversion library in ValueFormatter (BRAINSTORM-2166)
- Some options deprecated and corresponding tests removed

* Fri Oct 29 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.10.29-1.fmi
- Repackaged due to base grid library ABI changes

* Fri Oct 29 2021 Pertti Kinnia <pertti.kinnia@fmi.fi> - upcoming
- Added test dependency for smartmet-library-newbase-devel

* Tue Oct 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.10.19-1.fmi
- Repackaged due to ABI changes in base grid libraries

* Tue Oct 12 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.10.12-1.fmi
- DEM-height added to location object in observation query (BRAINSTORM-2169)

* Mon Oct 11 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.10.11-1.fmi
- Simplified grid storage structures

* Mon Oct  4 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.10.4-1.fmi
- Repackaged due to grid-files ABI changes

* Thu Sep 23 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.9.23-1.fmi
- Repackage to prepare for installing libconfig in different directory

* Tue Sep 21 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.9.21-1.fmi
- Removed internal query caching as ineffective and error prone, rely on ETag based frontend caching instead

* Mon Sep 20 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.9.20-1.fmi
- Fixed QEngine query cache bug (BRAINSTORM-2143)

* Wed Sep 15 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.9.15-1.fmi
- Repackaged due to NetCDF related ABI changes in base libraries

* Thu Sep 9 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.9.9-1.fmi
- Fixed a timestep handling bug in observation query (BRAINSTORM-2146)
- New option 'grouplocations' added (BRAINSTORM-2135)

* Tue Sep  7 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.9.7-1.fmi
- Repackaged due to dependency changes (libconfig -> libconfig17)

* Mon Aug 30 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.8.30-1.fmi
- Cache counters added (BRAINSTORM-1005)
- Fixed step=0 bux (BRAINSTORM-2136)

* Sat Aug 21 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.21-1.fmi
- Repackaged due to LocalTimePool ABI changes

* Thu Aug 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.19-1.fmi
- Start using local time pool to avoid unnecessary allocations of local_date_time objects (BRAINSTORM-2122)

* Tue Aug 17 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.17-1.fmi
- Use the new shutdown API

* Thu Aug 5 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.8.5-1.fmi
- Added support for new mobile producer 'bk_hydrometa' (BRAINSTORM-2125)

* Mon Aug  2 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.2-1.fmi
- Repackaged since GeoEngine ABI changed by switching to boost::atomic_shared_ptr

* Mon Jul  5 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.7.5-1.fmi
- Update after moving DataFilter from obsengine to spine

* Tue Jun 29 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.29-2.fmi
- Repackaged since Observation::Engine::Settings now uses DataFilter instead of SQLDataFilter

* Tue Jun  8 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.8-1.fmi
- Repackaged due to memory saving ABI changes in base libraries

* Tue Jun  1 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.1-1.fmi
- Repackaged due to ABI changes in grid libraries

* Thu May 27 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.5.27-1.fmi
- Added support for areasource query option in order to handle geometries with the same name (BRAINSTORM-2073): 
Optional geometry_tables.additional_tables.name configuration parameter identifies a database schema/table/field. 
If the name has been defined it must be used in areasource URL-parameter in order to use geometries defined in 
that database schema/table/field.

* Tue May 25 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.25-1.fmi
- Improved parameter alias handling

* Fri May 21 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.21-1.fmi
- Repackaged due to QEngine API change

* Wed May 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.19-2.fmi
- Use FMI hash functions, boost::hash_combine produces too many collisions

* Tue May 11 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.11-1.fmi
- A negative precision setting now implies {fmt} is allowed to find the best precision setting

* Thu May 6 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.5.6-1.fmi
- Fixed output of aggregation of observations (BRAINSTORM-2055)
- If starttime and endtime are same and timestep has not been given, use minutes 
from starttime/endtime as timestep
- Time-independent special parameters (like stationname) must not be missing 
even if aggregation is done on a timestep with no data in database

* Mon Apr 12 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.4.12-1.fmi
- Support for groupareas option added (BRAINSTORM-2040)

* Thu Apr  1 2021 Pertti Kinnia <pertti.kinnia@fmi.fi> - 21.4.1-1.fmi
- Repackaged due to grid-files API changes

* Thu Mar 18 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.3.18-1.fmi
- Database table name configurable for mobile observations (BRAINSTORM-2022)
- Mobile observation testcases use cloud database
- Added testcases for fmi_iot producer

* Fri Mar  5 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.5-1.fmi
- Merged master and grid branches

* Wed Mar  3 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.3-1.fmi
- Grid-engine may now be disabled

* Mon Mar  1 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.1-1.fmi
- Prefer emplace_back over push_back when possible for speed

* Fri Feb 19 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.2.19-1.fmi
- Added support for FMISIDs,WMOs,LPNNs in forecast queries (BRAINSTORM-1848)
- Allow areas to be both polygons and multipolygons

* Thu Feb 18 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.18-1.fmi
- Repackaged due to newbase ABI changes

* Tue Feb 16 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.16-1.fmi
- Merged fixes and improvements from master

* Thu Feb 11 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.11-1.fmi
- Merged WGS84 branch

* Tue Feb 9 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.2.9-1.fmi
- Return HTTP response status code '408' when database timeout occurs (BRAINSTORM-2002)

* Wed Feb  3 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.3-1.fmi
- New optional configuration parameter 'prevent_observation_database_query' introduced, 
observation engine parameter interface changed (INSPIRE-914)
- Use time_t for speed

* Wed Jan 27 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.27-1.fmi
- Repackaged due to base library ABI changes

* Mon Jan 25 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.1.25-1.fmi
- Added inkeyword-parameter (BRAINSTORM-929)
- Check for duplicate areas in qengine query (BRAINSTORM-1987)

* Tue Jan 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.19-1.fmi
- Performance improvements

* Thu Jan 14 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.14-1.fmi
- Repackaged smartmet to resolve debuginfo issues

* Mon Jan 11 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.11-1.fmi
- Repackaged due to grid-files API changes

* Tue Jan  5 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.5-1.fmi
- Upgrade to fmt 7.1.3

* Mon Jan  4 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.4-1.fmi
- Upgraded to GDAL 3.2

* Thu Dec 17 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.12.17-1.fmi
- Check for duplicate areas in qengine query (BRAINSTORM-1987)

* Tue Dec 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.12.15-1.fmi
- Upgrade to pgdg12

* Thu Dec  3 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.12.3-1.fmi
- Repackaged due to library ABI changes

* Mon Nov 30 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.11.30-1.fmi
- Improved producer handling

* Tue Nov 24 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.11.24-1.fmi
- Repackaged due to library ABI changes

* Fri Oct 30 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.30-1.fmi
- Upgrade to FMT 7.1

* Wed Oct 28 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.10.28-1.fmi
- Rebuild due to fmt upgrade

* Thu Oct 22 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.10.22-1.fmi
- Time period in several observation test cases changed because of CircleCI (BRAINSTORM-1940)
- Use new faster GRIB API

* Thu Oct 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.15-1.fmi
- Repackaged due to library ABI changes
- Fixed a large number of clang analyzer warnings

* Wed Oct 14 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.14-1.fmi
- Use new TableFormatter API

* Tue Oct  6 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.6-1.fmi
- Enable sensible relative libconfig include paths

* Wed Sep 23 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.23-1.fmi
- Use Fmi::Exception instead of Spine::Exception

* Fri Sep 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.18-1.fmi
- Repackaged due to library ABI changes

* Tue Sep 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.15-1.fmi
- Repackaged due to library ABI changes

* Mon Sep 14 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.14-1.fmi
- Repackaged due to library ABI changes

* Mon Sep  7 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.7-1.fmi
- Repackaged due to library ABI changes

* Tue Sep  1 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.8.20-1.fmi
- Support for both 'itmf' and 'fmi_iot' producer names
- Configuration files for regression tests updated

* Mon Aug 31 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.31-1.fmi
- Repackaged due to library ABI changes

* Fri Aug 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.21-2.fmi
- Use Fmi::to_string instead of std::string for speed

* Fri Aug 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.21-1.fmi
- Upgrade to fmt 6.2

* Tue Aug 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.18-1.fmi
- Speed improvements

* Fri Aug 14 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.14-1.fmi
- Repackaged due to grid library ABI changes

* Tue Aug 11 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.11-1.fmi
- Speed improvements

* Mon Jul 27 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.27-1.fmi
- Proceed with obs requests even if the station is not known to geonames

* Tue Jul 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.21-1.fmi
- Fixed geoid handling to generate the geoid as the response for the place parameter

* Mon Jul 20 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.20-1.fmi
- Fixed processing of geoid options to work similarly to coordinate searches instead of station searches

* Thu Jun 25 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.25-1.fmi
- Added debug querystring option

* Mon Jun 22 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.22-1.fmi
- Rebuilt due to base library changes

* Mon Jun 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.15-1.fmi
- Renamed .so file to enable simultaneous installation of timeseries and gribtimeseries

* Wed Jun 10 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.6.10-1.fmi
- Rebuilt due to obsengine API change

* Tue Jun  9 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.9-1.fmi
- Do not do a coordinate search for locations which already have a known fmisid

* Mon Jun 8 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.6.8-1.fmi
- Support for itmf-producer (INSPIRE-909)
- Support for PostgresSQL-driver in observation engine: configuration file structure changed (BRAINSTORM-1783)

* Tue May 26 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.5.26-1.fmi
- Modified test cases to include percentage-function in order to test parameter parsing
- Fixed bug in settings handling

* Tue May 26 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.5.26-1.fmi
- Test case added for different interval separator characters in time aggregation function (BRAINSTORM-1828)
- In addition to ':'-character also '/'- and ';'-characters are now supported

* Fri May 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.5.15-1.fmi
- Repackaged due to base library changes

* Tue May 12 2020  Anssi Reponen <anssi.reponen@fmi.fi> - 20.5.12-1.fmi
- Observation-engine API changed (BRAINSTORM-1678)
- Added support for data_quality option (BRAINSTORM-1706)
- New test cases and corrected test case results

* Thu Apr 30 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.30-1.fmi
- Repackaged due to base library API changes

* Sun Apr 26 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.26-1.fmi
- Repackaged

* Sat Apr 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.18-1.fmi
- Upgraded to Boost 1.69

* Fri Apr  3 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.3-1.fmi
- Repackaged due to library API changes

* Thu Apr 2 2020  Anssi Reponen <anssi.reponen@fmi.fi> - 20.4.2-1.fmi
- Using faster algorithm to add missing timesteps to time series (BRAINSTORM-1800)
- You can now use data_quality field as URL-parameter for NetAtmo producer (BRAINSTORM-1799)

* Mon Mar 30 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.3.30-1.fmi
- Full repackaging of the server

* Tue Mar 10 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.3.10-1.fmi
- Update using parameter tools from smartmet-library-spine (is_time_parameter)

* Thu Mar  5 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.3.5-1.fmi
- Use parameter tools from smartmet-library-spine and remove local version

* Tue Feb 25 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.25-2.fmi
- Repackaged due to base library API changes

* Fri Feb 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.21-1.fmi
- Upgrade to GDAL 3.0

* Thu Feb 20 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.20-2.fmi
- Fixed producer parser to check if the producer is known to GRIB-engine

* Thu Feb 20 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.2.20-1.fmi
- Fixed the 1-minute timestep (BRAINSTORM-1767)

* Tue Feb 18 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.2.18-1.fmi
- Use coordinates from geonames database (instead of geometry database) when place-option is used (BRAINSTORM-1757)

* Fri Feb 14 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.14-1.fmi
- Added a check for missing ObsEngine when validating producer names

* Thu Feb 13 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.13-1.fmi
- Use a system installed observation database in the tests
- Repackaged due to ABI changes

* Sun Feb  9 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.9-1.fmi
- Repackaged due to delfoi/obsengine changes

* Fri Feb  7 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.7-1.fmi
- Repackaged since Spine::Station default construction changed

* Wed Feb 5 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.2.5-1.fmi
- Interpolate- and nearest-functions added (BRAINSTORM-1504)
- Aggregation-related bugs fixed (BRAINSTORM-1755)
- Additional timesteps with NaN values in observation query resultset fixed (BRAINSTORM-1750)

* Thu Jan 30 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.1.30-1.fmi
- Fixed initialization of WKT geometries bug which caused a crash
- Check given producer names are valid

* Wed Jan 29 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.1.29-1.fmi
- Added a possibility to restrict grid requests without producer information
- Ensuring that newbase producers with alias names are accepted as grid producers

* Thu Jan 23 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.1.23-2.fmi
- Configuration file structure and reading changed bacause of gis-engine
interface changed (BRAINSTORM-1746)

* Tue Jan 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.1.21-1.fmi
- Repackaged due to grid-content and grid-engine API changes

* Thu Jan 16 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.1.16-1.fmi
- Make sure producer cache is updated frequently

* Fri Dec 13 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.13-1.fmi
- Upgrade to GDAL 3.0

* Wed Dec  4 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.4-1.fmi
- Repackaged due to base library changes

* Fri Nov 22 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.22-1.fmi
- Repackaged due to API changes in grid-content library

* Wed Nov 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.20-1.fmi
- Rebuilt due to newbase API changes
- More fixes for grid-parameter alias name bug (BRAINSTORM-1726)

* Tue Nov 19 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.11.19-1.fmi
- Changes from qd-timeseries imported. Fixed grid-parameter alias name bug (BRAINSTORM-1726)

* Thu Nov 14 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.11.14-1.fmi
- Refactoring code because textgenplugin must also support bbox and wkt parameters (BRAINSTORM-1720)

* Thu Nov  7 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.7-1.fmi
- Support for alternative query parameters

* Thu Oct 31 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.31-1.fmi
- Rebuild due to newbase API/ABI changes

* Wed Oct 30 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.30-1.fmi
- Full repackaging of GRIB server components

* Mon Oct 21 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.10.21-1.fmi
- Throw an exception if the requested station is not available in database (BRAINSTORM-1702)

* Tue Oct  1 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.1-1.fmi
- Repackaged due to SmartMet library ABI changes

* Thu Sep 26 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.26-1.fmi
- Added support for ASAN & TSAN builds

* Mon Sep 23 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.23-1.fmi
- Fixed not to link grid-libraries

* Fri Sep 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.20-1.fmi
- Repackaged all with -fno-omit-frame-pointer for better profiling

* Thu Sep 19 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.19-1.fmi
- New release version

* Tue Sep 17 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.9.17-1.fmi
- Test cases updated due to NatAtmo parameter name change (SOL-8557)
- NetAtmo, RoadCloud test cases updated because result set row order has 
change, content remains unchanged. Related to BRAINSTORM-1673.
- Test cases updated due to NatAtmo parameter name change (SOL-8557)

* Thu Sep  5 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.8.5-1.fmi
- Fixed error messages to use the same latlon order as the input parameters

* Wed Aug 28 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.8.28-1.fmi
- Repackaged since Spine::Location ABI changed
- Keywords now work even for stations with duplicate coordinates

* Sat Jun  8 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.6.8-1.fmi
- Avoid unnecessary string copies for speed

* Thu May 23 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.5.23-1.fmi
- Bugfix: precision-option applied to latitude parameter
- Name of mobileAndExternalDataFilter data member changed to dataFilter since it is used also in sounding-query
- Adding sounding_type to query options (related to BRAINSTORM-1359)
- Enabled observation-engine parameter useDataCache. When this option is set false data is fetched from original database instead of cache

* Wed May 15 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.5.15-1.fmi
- Fixed error in result set handling of observations fetched from cache (stroke_time)

* Thu May 2 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.5.2-1.fmi
- Return missing-values for unknown observation parameters (BRAINSTORM-1520)
- Enable aggregation of the following metaparameters: sundeclination,
sunelevation,sunazimuth,daylength (BRAINSTORM-1581)

* Tue Apr 23 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.4.23-1.fmi
- Support for mobile and external producers
- Producer name check made case insensitive
- Test cases for new mobile producers (NetAtmo, RoadCloud)

* Mon Mar 18 2019 Santeri Oksman <santeri.oksman@fmi.fi> - 19.3.18-1.fmi
- Add support to data independent parameters in observation queries

* Thu Feb 14 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.14-1.fmi
- Added client IP to exception reports

* Tue Dec  4 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.12.4-1.fmi
- Added paging (BS-1430)

* Fri Nov  9 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.11.9-2.fmi
- Fixed handling of comma-separated observation producers (BRAINSTORM-667)

* Fri Nov  9 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.11.9-1.fmi
- Support for data_source-field added (BRAINSTORM-1233)

* Thu Nov  8 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.8-1.fmi
- Do not throw in destructors in C++11

* Sat Sep 29 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.29-1.fmi
- Upgraded to latest fmt

* Wed Sep 19 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.9.19-1.fmi
- Improved handling-algorithm of NFmiSvgPath object

* Tue Sep 4 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.9.4-1.fmi
- Refactoring: data-functions moved to separate file

* Tue Aug 28 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.8.28-1.fmi
- Oracle parameter names in test/cnf/observation.conf file made uppercase (BRAIN

* Wed Aug 15 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.8.15-1.fmi
- Remove duplicate code, start using Fmi::OGR::createFromWkt-function

* Mon Aug 13 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.8.13-1.fmi
- Support for 'wkt' parameter added

* Fri Jul 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.27-1.fmi
- Refactored code for easier maintenance

* Wed Jul 25 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.25-1.fmi
- Optimized result generation code for speed by using references, emplace_back etc
- Prefer nullptr over NULL

* Mon Jul 23 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.23-1.fmi
- Repackaged since spine ValueFormatter ABI changed

* Thu May 24 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.24-2.fmi
- Default cache.filesystem_bytes value is now zero, the file cache is rarely needed

* Thu May 24 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.24-1.fmi
- Fixed time series generation to avoid generating excessive amounts of time steps for aggregation

* Wed May 16 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.16-1.fmi
- Fixed error handling when the format option is given more than once

* Wed Apr 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.11-1.fmi
- Don't warn on empty output, many flash data requests have empty responses

* Sat Apr  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.7-1.fmi
- Upgrade to boost 1.66

* Thu Apr  5 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.5-1.fmi
- Optimized speed of bbox queries by reducing geonames lookups

* Wed Apr  4 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.4-1.fmi
- Fixed fmisid extraction from timeseries to ignore gaps in actual observations

* Wed Mar 21 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.21-1.fmi
- SmartMetCache ABI changed

* Tue Mar 20 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.20-1.fmi
- Full recompile of all server plugins

* Mon Mar 19 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.19-1.fmi
- Removed obsolete call to Observation::Engine::setGeonames

* Thu Mar  1 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.1-1.fmi
- Avoid locale copying in case conversions

* Tue Feb 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.27-1.fmi
- Querydata engine API was changed to be const correct

* Fri Feb  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.9-1.fmi
- Repackaged due to macgyver TimeZones API change

* Mon Jan 15 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.15-1.fmi
- Removed libpqxx linkage

* Thu Nov 30 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.11.30-1.fmi
- Start using GisEngine instead of PostGISDataSource class (BRAINSTORM-722)

* Sat Oct 28 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.28-1.fmi
- Allow STUK and FINAVIA stations in addition to SYNOP stations

* Mon Aug 28 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.28-1.fmi
- Upgrade to boost 1.65

* Fri Aug 18 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.18-1.fmi
- Plain ETag response code to frontend is now 204 no content
- Fixed content hash code not to depend on the wall clock

* Mon Jul 17 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.7.17-1.fmi
- Downgraded newbase dependency, a development version was installed in the build machine

* Mon Jul 10 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.7.10-1.fmi
- default locale is now an obligatory configuration variable
- default language is now an obligatory configuration variable

* Sat Apr  8 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.8-1.fmi
- Simplified error reporting
- sid is now an alias for fmisid

* Tue Apr  4 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.4-1.fmi
- Support for pressure and height value queries

* Mon Apr  3 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.3-1.fmi
- Added Access-Control-Allow-Origin: * header

* Wed Mar 15 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.15-2.fmi
- Do not log when no data is available for some location

* Wed Mar 15 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.15-1.fmi
- Recompiled since Spine::Exception changed

* Tue Mar 14 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.14-1.fmi
- Switched to use macgyver StringConversion tools 

* Sat Feb 11 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.11-1.fmi
- Repackaged due to newbase API changes

* Fri Jan 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.13-1.fmi
- Improved error reports for unknown timezone names

* Thu Jan 12 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.12-1.fmi
- Fixed observation_disabled=true to work

* Wed Jan  4 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.4-1.fmi
- Changed to use renamed SmartMet base libraries

* Wed Nov 30 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.30-1.fmi
- Using test databases in test configuration
- No installation for configuration
- Note: tests not yet working properly against test databases

* Tue Nov 29 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.29-1.fmi
- Fixed handling of starttime requests which predate the data start time

* Mon Nov 28 2016 Santeri Oksman <santeri.oksman@fmi.fi> - 16.11.28-1.fmi
- Added possibility to disable GIS database in configuration

* Fri Nov 11 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.1-1.fmi
- Namespace changed
- Aggregation bugfix BRAINSTORM-744: The requested timesteps are shown even if they differ from querydata native timesteps

* Tue Sep  6 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.6-1.fmi
- New exception handler

* Tue Aug 30 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.8.30-1.fmi
- Base class API change
- Use response code 400 instead of 503

* Mon Aug 15 2016 Markku Koskela <markku.koskela@fmi.fi> - 16.8.15-1.fmi
- The init(),shutdown() and requestHandler() methods are now protected methods
- The requestHandler() method is called from the callRequestHandler() method
- Fixed a bug in handling starttime=data

* Wed Jun 29 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.29-1.fmi
- Default maxdistance is now 60 km, which is suitable for observations
- If no maxdistance setting is given, enable data specific limit configured in qengine

* Mon Jun 20 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.20-1.fmi
- Fixed starttime handling for weekly data (BRAINSTORM-692)

* Tue Jun 14 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.14-1.fmi
- Full recompile

* Mon Jun  6 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.6-1.fmi
- Fixed WXML schema location (SOL-3969)

* Thu Jun  2 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.2-2.fmi
- Fixed cache settings to be of type long

* Thu Jun  2 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.2-1.fmi
- Full recompile

* Wed Jun  1 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.1-1.fmi
- Added graceful shutdown

* Mon May 16 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.5.16-1.fmi
- Replaced time zone names with time zone objects when calling the TimeSeriesGenerator

* Wed May 11 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.5.11-1.fmi
- Replaced TimeZoneFactory singleton with TimeZones member variable to avoid mutexes
- Optimized the default locale initialization

* Wed Apr 27 2016 Santeri Oksman <santeri.oksman@fmi.fi> - 16.4.27-1.fmi
- Fixes to flash queries

* Wed Apr 20 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.4.20-1.fmi
- Skip bounding box generation if we are getting flash data.
- Fixed timestep=data to work for climatological data too

* Mon Apr 11 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.4.11-1.fmi
- Fixed empty response issue when using timestep=data

* Tue Apr  5 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.4.5-1.fmi
- merivaroitusalueet added to timeseries.conf
- swc_merialueet added to timeseries.conf

* Mon Feb 22 2016  Santeri Oksman <santeri.oksman@fmi.fi> - 16.2.22-1.fmi
- Various fixes to timeseries generation in case of observations.

* Wed Feb 10 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.10-1.fmi
- Fixed missing observations issue

* Tue Feb  9 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.9-2.fmi
- Fixed issues in timestamp timezone handling

* Tue Feb  9 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.9-1.fmi
- Rebuilt against the new TimeSeries::Value definition

* Tue Feb  2 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.2-1.fmi
- Now using Timeseries None - type

* Sat Jan 23 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.23-1.fmi
- Fmi::TimeZoneFactory API changed

* Mon Jan 18 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.18-1.fmi
- newbase API changed, full recompile

* Fri Jan 15 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.15-1.fmi
- An empty result from obsengine is no longer an error

* Mon Jan 11 2016 Santeri Oksman <santeri.oksman@fmi.fi> - 16.1.11-1.fmi
- Move parameterIsArithmetic check from qengine to timeseries.

* Wed Nov 18 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.11.18-1.fmi
- SmartMetPlugin now receives a const HTTP Request

* Tue Nov 10 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.11.10-1.fmi
- Avoid string streams to avoid global std::locale locks

* Mon Nov  9 2015 Santeri Oksman <santeri.oksman@fmi.fi> - 15.11.9-1.fmi
- Fixed missing flash data

* Mon Oct 26 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.10.26-1.fmi
- Added proper debuginfo packaging

* Thu Oct 22 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.9.22-1.fmi
- Fixed wxml tag text

* Wed Sep 16 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.9.16-1.fmi
- Use geoengine->nameSearch instead of suggest when searching for observation stations.

* Mon Aug 31 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.31-1.fmi
- Improved caching by analyzing the generated times only, not the options generating it

* Fri Aug 28 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.28-1.fmi
- Faster queries when multiple locations are requested

* Thu Aug 27 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.27-1.fmi
- Time series generation is now cached for speed

* Wed Aug 26 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.26-1.fmi
- Fixed wmo requests to work

* Tue Aug 25 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.25-1.fmi
- Optimized observation queries for speed.

* Mon Aug 24 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.24-1.fmi
- Recompiled due to Convenience.h API changes

* Wed Aug 19 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.8.19-1.fmi
- Fixed location passing to ObsEngine

* Tue Aug 18 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.18-1.fmi
- Use time formatters from macgyver to avoid global locks from sstreams

* Mon Aug 17 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.17-2.fmi
- Use -fno-omit-frame-pointer to improve perf use

* Mon Aug 17 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.17-1.fmi
- Optimized time period initializer for speed

* Fri Aug 14 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.14-1.fmi
- Avoid boost::lexical_cast, Fmi::number_cast and std::ostringstream for speed

* Tue Aug  4 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.8.4-1.fmi
- Optimizations
- Place parameter works with observations

* Thu Jul 30 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.7.30-1.fmi
- Added caching and ETag-handling

* Fri Jun 26 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.26-1.fmi
- Recompiled due to spine API changes

* Tue Jun 23 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.23-1.fmi
- Location API changed

* Mon May 25 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.5.25-1.fmi
- Added 'merialueet' - areas
- Removed Obsengine-specific parameter parsing. It was a source for confusion.
- Added nan-ignoring aggregation functions

* Fri May 22 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.5.22-1.fmi
- Region was wrong when making pure geographical queries

* Wed Apr 29 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.29-1.fmi
- Removed OptionEngine dependency
- PostGIS data source is no longer case sensitive
- Better output result set identification when requesting several timeseries

* Fri Apr 17 2015 Santeri Oksman <santeri.oksman@fmi.fi> - 15.4.17-1.fmi
- Check against empty obsengine result

* Thu Apr 16 2015 Santeri Oksman <santeri.oksman@fmi.fi> - 15.4.16-2.fmi
- Fix timezone handling in observation queries when tz=localtime

* Thu Apr 16 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.16-1.fmi
- Fixed origintime handling in qengine queries

* Tue Apr 14 2015 Santeri Oksman <santeri.oksman@fmi.fi> - 15.4.14-2.fmi
- Fix missing origintime handling in observation queries

* Tue Apr 14 2015 Santeri Oksman <santeri.oksman@fmi.fi> - 15.4.14-1.fmi
- New release to enable observation cache

* Thu Apr  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.9-1.fmi
- newbase API changed

* Wed Apr  8 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.8-1.fmi
- Dynamic linking of smartmet libraries

* Mon Mar 23 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.23-1.fmi
- Saner location object copying

* Mon Mar  9 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.9-1.fmi
- Added flight path aggregation areas

* Tue Feb 24 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.2.24-1.fmi
- Recompiled due to newbase linkage changes

* Wed Feb 18 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.2.18-1.fmi
- Now distinguishing between fast and slow query types

* Tue Feb 17 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.2.17-1.fmi
- Fixed region-parameter handling in pure geographical searches

* Tue Jan 13 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.1.13-1.fmi
- Fixed origintime option to work
- Added origintime=latest, origintime=newest and origintime=oldest queries

* Wed Jan  7 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.1.7-1.fmi
- Recompiled due to obsengine API changes

* Thu Dec 18 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.12.18-1.fmi
- Recompiled due to spine API changes

* Fri Nov 14 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.11.14-1.fmi
- Fixed jira issues 'brainstorm-389', 'brainstorm-391': Bug in aggregation and handling of localtime/utctime
- regression test added

* Fri Oct 24 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.10.24-1.fmi
- Removed pointless reporting

* Mon Oct  6 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.10.6-1.fmi
- Migrated from sartre to popper-gis

* Tue Sep 16 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.9.16-1.fmi
- Fixed segfault issue

* Mon Sep  8 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.8-1.fmi
- Recompiled due to geoengine API changes

* Tue Aug 26 2014 Anssi Reponen <anssi.reponen@fmi.fi> - 14.8.26-2.fmi
- Corrected handling of missing values during aggregation
- Corrected handling of timesteps in the beginning and in the end of timeseries
- Regression tests updated

* Tue Aug 26 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.8.26-1.fmi
- Hotfixed NaN handling in aggregates

* Mon Jun 30 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.30-1.fmi
- Recompiled with latest obsengine API

* Wed May 21 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.5.21-1.fmi
- Added road areas to aggregation shapes.

* Wed May 14 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.14-1.fmi
- Use shared macgyver and locus libraries

* Wed May  7 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.7-2.fmi
- Models are now specified in a time sequence of model groups for different areas
- For example: hirlam;pal,ecmwf takes all of HIRLAM first, then PAL if available for the location, otherwise ECMWF. Use endtime=data for best effect, feature is not perfect yet.

* Wed May  7 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.7-1.fmi
- Fixed endtime=data to work

* Tue May  6 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.5.6-1.fmi
- Recompiled due to qengine API changes

* Mon Apr 28 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.4.28-1.fmi
- Full recompile due to large changes in spine etc APIs

* Thu Apr 10 2014 Anssi Reponen <anssi.reponen@fmi.fi> 14.4.10-1.fmi
- ObsEngine support added
- Modifications because of QEngine API change

* Wed Feb 19 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.2.19-1.fmi
- Added precision definitions for precipitation fractile parameters

* Wed Feb  5 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.2.5-1.fmi
- Fixed handling of startime=data and endtime=data
- Added time offset forwards for aggregation
- Added Integ function

* Wed Dec  4 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.12.4-1.fmi
- Aggregation is now properly disabled for special parameters which are not of arithmetic type

* Mon Nov 25 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.25-1.fmi
- Sign fix to geoid

* Thu Nov 14 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.14-1.fmi
- Updated to use Locus library

* Tue Nov  5 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.11.5-1.fmi
- Major release

* Wed Oct  9 2013 Tuomo Lauri <tuomo.lauri@fmi.fi> - 13.10.9-1.fmi
- Now conforming with the new Reactor initialization API
- Started using NFmiMultifileInfo class. Test cases added for NFmiMultifileInfo class.
- Plugin now accepts both 'producer' and 'model' query parameters as model indicators

* Mon Sep 23 2013 Tuomo Lauri    <tuomo.lauri@fmi.fi>    - 13.9.23-1.fmi
- Recompile due to QEngine API changes
- The plugin is now initialized in a separate thread

* Fri Sep 6 2013 Tuomo Lauri   <tuomo.lauri@fmi.fi>    - 13.9.6-1.fmi
- Fixed Wxml formatting issues

* Tue Sep 3 2013 Anssi Reponen <anssi.reponen@fmi.fi>  - 13.9.3-1.fmi
- Default missing text for wxml timestring parameter changed to empty string 

* Wed Aug 28 2013 Tuomo Lauri   <tuomo.lauri@fmi.fi>   - 13.8.28-1.fmi
- Aggregation-related fixes

* Tue Aug 27 2013 Anssi Reponen <anssi.reponen@fmi.fi> - 13.8.27-1.fmi
- Hotfix: No area operation allowed for non-grid data

* Tue Jul 23 2013 Mika Heiskanen <mika.heiskanen@fmi.fi> - 13.7.23-1.fmi
- Recompiled due to thread safety fixes in newbase & macgyver

* Wed Jul  3 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.7.3-1.fmi
- Update to boost 1.54

* Mon Jun 17 2013 lauri    <tuomo.lauri@fmi.fi>    - 13.6.17-1.fmi
- Fixed code to error if a keyword contains places outside the selected model areas

* Tue Jun  4 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.6.4-2.fmi
- Fixed code to error if no producer can be selected

* Tue Jun  4 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.6.4-1.fmi
- Merged areaforecast and pointforecast plugins into timeseries plugin

* Mon Jun  3 2013 lauri <tuomo.lauri@fmi.fi> - 13.6.3-1.fmi
- Rebuilt against the new Spine

* Fri May 24 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.5.24-1.fmi
- Fixes to time series generator defaults on timezones and start times
- Plugin no longer responds with 503-status code, it's bad behaviour

* Wed May 22 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.5.22-1.fmi
- Improved time series generator

* Mon Apr 22 2013 mheiskan <mika.heiskanen@fmi.fi> - 13.4.22-1.fmi
- Brainstorm API changed

* Fri Apr 12 2013 lauri <tuomo.lauri@fmi.fi>    - 13.4.12-1.fmi
- Rebuild due to changes in Spine

* Wed Feb  6 2013 lauri    <tuomo.lauri@fmi.fi>    - 13.2.6-1.fmi
- Built against new Spine and Server

* Mon Nov 19 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.11.19-1.el6.fmi
- Added origintime option

* Wed Nov  7 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.11.7-1.el6.fmi
- Upgrade to boost 1.52
- Upgrade to refactored spine library

* Tue Sep 18 2012 lauri    <tuomo.lauri@fmi.fi>    - 12.9.18-1.el6.fmi
- Recompile due to changes in macgyver

* Tue Aug  7 2012 lauri    <tuomo.lauri@fmi.fi>    - 12.8.7-1.el6.fmi
- Location API changed

* Tue Jul 31 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.31-1.el6.fmi
- GeoNames API changed

* Thu Jul 26 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.26-1.el6.fmi
- GeoNames API changed

* Tue Jul 24 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.24-1.el6.fmi
- GeoNames API changed

* Mon Jul 23 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.23-1.el6.fmi
- Added ApparentTemperature

* Thu Jul 19 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.19-1.el6.fmi
- GeoNames API changed

* Tue Jul 10 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.10-1.el6.fmi
- Recompiled since Table changed

* Thu Jul  5 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.7.5-1.el6.fmi.fi
- Upgrade to boost 1.50

* Fri Jun  8 2012 oksman <santeri.oksman@fmi.fi> - 12.6.8-1.el6.fmi.fi
- Recompile forced due fixes in JsonFormatter.

* Tue Apr 10 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.10-1.el5.fmi
- Fixed WXML output not to print "nan" as a value field

* Wed Apr  4 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.6-1.el6.fmi
- full recompile due to common lib change

* Wed Apr  4 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.4-1.el6.fmi
- qengine API changed

* Mon Apr  2 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.4.2-1.el6.fmi
- macgyver change forced recompile

* Sat Mar 31 2012 mheiskan <mika.heiskanen@fmi.fi> - 12.3.31-1.el5.fmi
- Upgrade to boost 1.49

* Tue Dec 27 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.12.27-2.el5.fmi
- Table class changed, recompile forced

* Wed Dec 21 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.12.12-1.el6.fmi
- RHEL6 release

* Tue Aug 16 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.8.16-1.el5.fmi
- Upgrade to boost 1.47

* Thu Mar 24 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.3.24-1.el5.fmi
- Upgrade to boost 1.46

* Mon Feb 14 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.2.14-1.el5.fmi
- Recompiled to use OptionEngine

* Tue Jan 18 2011 mheiskan <mika.heiskanen@fmi.fi> - 11.1.18-1.el5.fmi
- Refactored query string parsing

* Fri Nov 26 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.11.26-1.el5.fmi
- Added cache headers

* Thu Oct 28 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.10.28-1.el5.fmi
- Recompile due to external API changes

* Wed Oct 27 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.10.27-1.el5.fmi
- Fixed DST problems again

* Wed Oct 20 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.10.20-1.el5.fmi
- Fixed DST problems

* Wed Sep 22 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.9.22-1.el5.fmi
- Improved error messages to the terminal

* Tue Sep 14 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.9.14-1.el5.fmi
- Upgrade to boost 1.44

* Mon Aug  9 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.8.9-1.el5.fmi
- Updated GeoEngine

* Wed Jul 21 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.7.21-3.el5.fmi
- xml.tag is now supported in pointforecast.conf
- Added WindChill parameter

* Tue Jul  6 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.7.6-1.el5.fmi
- Wxml formatting now includes location coordinates

* Mon Jul  5 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.7.5-1.el5.fmi
- Recompile brainstorm due to newbase hessaa bugfix

* Thu May 27 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.5.27-1.el5.fmi
- Zero size responses are now reported to the terminal

* Wed May  5 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.5.5-1.el5.fmi
- Fixed DST problem with Cairo

* Thu Apr 29 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.4.29-1.el5.fmi
- Moved formatters to common library

* Tue Apr 20 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.4.20-1.el5.fmi
- Fixed timesteps to work correctly when time or hour option is used

* Tue Apr 13 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.4.13-1.el5.fmi
- Fixed JSON mimetype

* Sat Mar 27 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.3.27-1.el5.fmi
- Fixed another DST problem when constructing midnight times

* Wed Mar 24 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.3.24-1.el5.fmi
- Fixed DST problem when the hour option is used

* Tue Mar 23 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.3.23-1.el5.fmi
- Added meta parameter Cloudiness8th

* Tue Feb 16 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.2.16-1.el5.fmi
- Enabled "model=default" selection

* Fri Jan 15 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.1.15-1.el5.fmi
- Upgrade to boost 1.41

* Mon Jan 11 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.1.12-1.el5.fmi
- Bug fix to timestep=graph when starttime is not given

* Mon Jan 11 2010 mheiskan <mika.heiskanen@fmi.fi> - 10.1.11-1.el5.fmi
- Added support for timestep=graph

* Wed Dec 30 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.12.30-1.el5.fmi
- Fixed bug in WindCompass8

* Tue Dec 29 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.12.29-1.el5.fmi
- Added WindCompass8, WindCompass16 and WindCompass32

* Fri Dec 11 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.12.11-1.el5.fmi
- Added startstep=N option for skipping forward in the forecast data

* Wed Dec  9 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.12.9-1.el5.fmi
- Added iso2 parameter

* Tue Dec  8 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.12.8-1.el5.fmi
- Fixed json formatting of arrays

* Thu Nov 26 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.11.26-2.el5.fmi
- Set SigWaveHeight normal precision to 1 decimal

* Thu Nov 26 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.11.26-1.el5.fmi
- Added latlon and lonlat parameters to pointforecast

* Mon Nov 23 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.11.23-1.el5.fmi
- Added conversion of weekdays to UTF8

* Thu Oct 29 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.10.29-1.el5.fmi
- Recompile with new qengine

* Wed Jul 22 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.7.22-1.el5.fmi
- QEngine API changed forced recompile

* Tue Jul 14 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.7.14-1.el5.fmi
- Upgrade to boost 1.39

* Fri Mar 27 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.3.27-1.el5.fmi
- hour option did not work correctly over DST changes

* Wed Mar 25 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.3.25-1.el5.fmi
- Fixed timestep, previously only multiples of 60 worked
- Fixed epochtime calculation

* Thu Feb  5 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.2.5-1.el5.fmi
- Fixed wxml formatting not to crash if no places are given

* Mon Jan 26 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.1.26-2.el5.fmi
- Added starttime=data and endtime=data options

* Mon Jan 26 2009 mheiskan <mika.heiskanen@fmi.fi> - 9.1.26-1.el5.fmi
- Location name is now searched for latlon coordinates
- Fixed WXML formatting

* Fri Dec 19 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.19-1.el5.fmi
- Newbase parameter names are now case independent

* Tue Dec 16 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.16-4.el5.fmi
- Added maxdistance parameter

* Tue Dec 16 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.16-3.el5.fmi
- No error is generated if data does not cover given places in keyword mode

* Fri Dec 12 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.12-2.el5.fmi
- WXML timeformat is now forced to be XML

* Fri Dec 12 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.12-1.el5.fmi
- Fixed PHP and serial formatting
- Added origintime parameter
- Added origintime to WXML
- Improved error handling

* Wed Dec 10 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.10-1.el5.fmi
- Updates to level handling
- New fminames, geoengine, qengine

* Mon Dec  8 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.8-2.el5.fmi
- Fixed xml header
- Fixed pointforecast mimetypes

* Thu Dec  4 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.4-1.el5.fmi
- Fixed bug in WxmlFormatter

* Wed Dec  3 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.12.3-1.el5.fmi
- Fixed country searches not to prioritize Finland first

* Thu Oct 23 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.10.23-1.el5.fmi
- Linked with updated macgyver library

* Mon Oct  6 2008 mheiskan <mika.heiskanen@fmi.fi> - 8.10.6-1.el5.fmi
- First release
