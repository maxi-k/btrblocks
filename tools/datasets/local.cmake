# ---------------------------------------------------------------------------
# BtrBlocks Dataset Analysis Tools
# ---------------------------------------------------------------------------

set(BTR_DATASET_DIR ${CMAKE_CURRENT_LIST_DIR})

# ---------------------------------------------------------------------------
# Collect stats for individual data types
# ---------------------------------------------------------------------------

include("${BTR_DATASET_DIR}/integer-stats/local.cmake")
include("${BTR_DATASET_DIR}/double-stats/local.cmake")
include("${BTR_DATASET_DIR}/string-stats/local.cmake")

# ---------------------------------------------------------------------------
# Individual scripts
# ---------------------------------------------------------------------------

configure_file(${BTR_DATASET_DIR}/integer_stats.py integer_stats.py COPYONLY)
configure_file(${BTR_DATASET_DIR}/double_stats.py double_stats.py COPYONLY)
