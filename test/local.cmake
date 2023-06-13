# ---------------------------------------------------------------------------
# BtrBlocks Tests
# ---------------------------------------------------------------------------

set(BTR_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})
set(BTR_TEST_CASES_DIR ${BTR_TEST_DIR}/test-cases)

# ---------------------------------------------------------------------------
# Files
# ---------------------------------------------------------------------------

file(GLOB_RECURSE TEST_CASES ${BTR_TEST_CASES_DIR}/**.cpp ${BTR_TEST_CASES_DIR}/**.hpp)

# ---------------------------------------------------------------------------
# Tester
# ---------------------------------------------------------------------------

add_executable(tester ${BTR_TEST_DIR}/tester.cpp ${TEST_CASES})
target_link_libraries(tester btrblocks gtest gmock Threads::Threads)
target_include_directories(tester PRIVATE ${BTR_INCLUDE_DIR})
target_include_directories(tester PRIVATE ${BTR_TEST_DIR})

enable_testing()

# ---------------------------------------------------------------------------
# Test Dataset Generator
# ---------------------------------------------------------------------------

add_executable(test_dataset_generator ${BTR_TEST_DIR}/DatasetGenerator.cpp)
target_link_libraries(test_dataset_generator btrblocks gflags Threads::Threads)
target_include_directories(test_dataset_generator PRIVATE ${BTR_INCLUDE_DIR})

# ---------------------------------------------------------------------------
# Linting
# ---------------------------------------------------------------------------

add_clang_tidy_target(lint_test "${TEST_CC}")
add_dependencies(lint_test gtest)
list(APPEND lint_targets lint_test)
