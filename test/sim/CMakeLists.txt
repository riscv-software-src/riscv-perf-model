project(olympia_test)

# Include the handy testing marcros defined in Sparta
include(${SPARTA_BASE}/test/TestingMacros.cmake)

# This line will make sure olympia is built before running the tests
sparta_regress (olympia)

# Create a few links like reports and arch directories for the testers
file(CREATE_LINK ${SIM_BASE}/reports ${CMAKE_CURRENT_BINARY_DIR}/reports SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/arches  ${CMAKE_CURRENT_BINARY_DIR}/arches SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/mavis/json ${CMAKE_CURRENT_BINARY_DIR}/mavis_isa_files SYMBOLIC)
file(CREATE_LINK ${SIM_BASE}/traces  ${CMAKE_CURRENT_BINARY_DIR}/traces SYMBOLIC)

# Tests
sparta_named_test(olympia_json_test olympia --workload traces/example_json.json)

# Test report generation

# This command will use a report definition file to generate a text form of a report
sparta_named_test(olympia_json_test_report_text olympia
  --report-yaml-replacements OUT_BASE json_test_report OUT_FORMAT text INST_START 0
  --report-search-dir reports
  --report reports/core_report.def
  --workload traces/example_json.json)

# This command will use core_stats.yaml directly to generate a text form of a report at the "top" node
sparta_named_test(olympia_json_test_report_text_direct olympia
  --report "top" reports/core_stats.yaml core_stats.text text
  --workload traces/example_json.json)

# This command will use core_stats.yaml directly to generate an html form of a report at the "top" node
sparta_named_test(olympia_json_test_report_html olympia
  --report "top" reports/core_stats.yaml core_stats.html html
  --workload traces/example_json.json)

# This command will run the dhrystone trace (1 million instructions)
sparta_named_test(olympia_dhry_test_report_html olympia -i 1M
  --report-yaml-replacements OUT_BASE dhry_test_report OUT_FORMAT text INST_START 0
  --report-search-dir reports
  --report reports/core_report.def
  --workload traces/dhrystone.zstf)
