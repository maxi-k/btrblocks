# ---------------------------------------------------------------------------
# BtrBlocks Tooling
# ---------------------------------------------------------------------------

set(BTR_TOOLS_DIR ${CMAKE_CURRENT_LIST_DIR})

# ---------------------------------------------------------------------------
# Task-specific tools
# ---------------------------------------------------------------------------

include("${BTR_TOOLS_DIR}/conversion/local.cmake")
include("${BTR_TOOLS_DIR}/datasets/local.cmake")
include("${BTR_TOOLS_DIR}/engine-comparison/local.cmake")
include("${BTR_TOOLS_DIR}/playground/local.cmake")
include("${BTR_TOOLS_DIR}/examples/local.cmake")
include("${BTR_TOOLS_DIR}/regression-benchmark/local.cmake")
