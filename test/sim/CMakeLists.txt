project(olympia_test)

# Include the handy testing marcros defined in Sparta
include(${SPARTA_BASE}/test/TestingMacros.cmake)

# This line will make sure olympia is built before running the tests
sparta_regress (olympia)

# Create a few links like reports and arch directories for the testers
file(CREATE_LINK ${SIM_BASE}/reports ${CMAKE_CURRENT_BINARY_DIR}/reports SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/archs   ${CMAKE_CURRENT_BINARY_DIR}/archs SYMBOLIC)

# Tests
sparta_named_test(max_ipc_1M     olympia -i 1M -p top.cpu.core0.fetch.params.fetch_max_ipc true)
sparta_named_test(random_ipc_1M  olympia -i 1M -p top.cpu.core0.fetch.params.fetch_max_ipc false)

# Test report generation

# This command will use a report definition file to generate a text form of a report
sparta_named_test(olympia_report_text olympia -i 1M -p top.cpu.core0.fetch.params.fetch_max_ipc true
  --report-yaml-replacements OUT_BASE max_ipc_report OUT_FORMAT text INST_START 1000
  --report-search-dir reports
  --report reports/core_report.def)

# This command will use core_stats.yaml directly to generate a text form of a report at the "top" node
sparta_named_test(olympia_report_text_direct olympia -i 1M -p top.cpu.core0.fetch.params.fetch_max_ipc true
  --report "top" reports/core_stats.yaml core_stats.text text)

# This command will use core_stats.yaml directly to generate an html form of a report at the "top" node
sparta_named_test(olympia_report_html olympia -i 1M -p top.cpu.core0.fetch.params.fetch_max_ipc true
  --report "top" reports/core_stats.yaml core_stats.html html)
