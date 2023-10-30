# ---------------------------------------------------------------------------
# BtrBlocks Benchmarks
# ---------------------------------------------------------------------------

set(BTR_BENCH_DIR ${CMAKE_CURRENT_LIST_DIR})

# ---------------------------------------------------------------------------
# Benchmark
# ---------------------------------------------------------------------------

add_executable(benchmarks ${BTR_BENCH_DIR}/benchmarks.cpp)
target_link_libraries(benchmarks btrblocks gbenchmark gtest gmock Threads::Threads)
target_include_directories(benchmarks PRIVATE ${BTR_INCLUDE_DIR})
target_include_directories(benchmarks PRIVATE ${BTR_BENCH_DIR})

enable_testing()

# ---------------------------------------------------------------------------
# Linting
# ---------------------------------------------------------------------------

add_clang_tidy_target(lint_bench "${BENCH_CC}")
add_dependencies(lint_bench gtest)
add_dependencies(lint_bench gbenchmark)
list(APPEND lint_targets lint_bench)

# ---------------------------------------------------------------------------
# Download Benchmark Data
# ---------------------------------------------------------------------------
set(public_bi_path "https://public-bi-benchmark.s3.eu-central-1.amazonaws.com/binary")
set(files_to_download
        "Arade/1/Arade_1/10_Number of Records.integer"
        "Arade/1/Arade_1/11_WNET (bin).integer"
        "Arade/1/Arade_1/1_F1.string"
        "Arade/1/Arade_1/2_F2.string"
        "Arade/1/Arade_1/4_F4.double"
        "Arade/1/Arade_1/5_F5.double"
        "Arade/1/Arade_1/6_F6.string"
        "Arade/1/Arade_1/7_F7.string"
        "Arade/1/Arade_1/8_F8.double"
        "Arade/1/Arade_1/9_F9.double"
        "Arade/1/Arade_1/10_Number of Records.bitmap"
        "Arade/1/Arade_1/11_WNET (bin).bitmap"
        "Arade/1/Arade_1/1_F1.bitmap"
        "Arade/1/Arade_1/2_F2.bitmap"
        "Arade/1/Arade_1/4_F4.bitmap"
        "Arade/1/Arade_1/5_F5.bitmap"
        "Arade/1/Arade_1/6_F6.bitmap"
        "Arade/1/Arade_1/7_F7.bitmap"
        "Arade/1/Arade_1/8_F8.bitmap"
        "Arade/1/Arade_1/9_F9.bitmap"
        "Bimbo/1/Bimbo_1/10_Semana.integer"
        "Bimbo/1/Bimbo_1/10_Semana.bitmap"
        "Bimbo/1/Bimbo_1/11_Venta_hoy.bitmap"
        "Bimbo/1/Bimbo_1/11_Venta_hoy.double"
        "Bimbo/1/Bimbo_1/12_Venta_uni_hoy.bitmap"
        "Bimbo/1/Bimbo_1/12_Venta_uni_hoy.integer"
        "Bimbo/1/Bimbo_1/1_Agencia_ID.bitmap"
        "Bimbo/1/Bimbo_1/1_Agencia_ID.integer"
        "Bimbo/1/Bimbo_1/2_Canal_ID.bitmap"
        "Bimbo/1/Bimbo_1/2_Canal_ID.integer"
        "Bimbo/1/Bimbo_1/3_Cliente_ID.bitmap"
        "Bimbo/1/Bimbo_1/3_Cliente_ID.integer"
        "Bimbo/1/Bimbo_1/4_Demanda_uni_equil.bitmap"
        "Bimbo/1/Bimbo_1/4_Demanda_uni_equil.integer"
        "Bimbo/1/Bimbo_1/5_Dev_proxima.bitmap"
        "Bimbo/1/Bimbo_1/5_Dev_proxima.double"
        "Bimbo/1/Bimbo_1/6_Dev_uni_proxima.bitmap"
        "Bimbo/1/Bimbo_1/6_Dev_uni_proxima.integer"
        "Bimbo/1/Bimbo_1/7_Number of Records.bitmap"
        "Bimbo/1/Bimbo_1/7_Number of Records.integer"
        "Bimbo/1/Bimbo_1/8_Producto_ID.bitmap"
        "Bimbo/1/Bimbo_1/8_Producto_ID.integer"
        "Bimbo/1/Bimbo_1/9_Ruta_SAK.bitmap"
        "Bimbo/1/Bimbo_1/9_Ruta_SAK.integer"
)

set(download_directory "${CMAKE_BINARY_DIR}/benchmark_data")

# Create the download directory if it doesn't exist
file(MAKE_DIRECTORY ${download_directory})

# Loop through the list of files and download them if they don't exist
foreach(file ${files_to_download})
    get_filename_component(filename "${file}" NAME)
    set(file_path "${download_directory}/${filename}")

    if(NOT EXISTS "${file_path}")
        message(STATUS "Downloading ${filename}")
        file(DOWNLOAD "${public_bi_path}/${file}" "${file_path}" SHOW_PROGRESS)
    else()
        message(STATUS "File ${filename} already exists. Skipping download.")
    endif()
endforeach()
