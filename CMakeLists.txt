cmake_minimum_required(VERSION 3.20)
project(volt_steth VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Otimizações de compilação
add_compile_options(-Wall -Wextra -Wpedantic -O3)

find_package(ALSA REQUIRED)

include_directories(
    include
    ${ALSA_INCLUDE_DIRS}
)

set(CORE_SOURCES
    src/config/AppConfig.cpp
    src/sources/AlsaAudioSource.cpp
    src/sources/HwmonPowerSource.cpp
    src/sources/PcieAerSource.cpp
    src/filters/GroundLoopDetector.cpp
    src/filters/VoltageRippleDetector.cpp
    src/filters/EmiCorrelationDetector.cpp
    src/sinks/ConsoleHudSink.cpp
    src/sinks/JsonStreamSink.cpp
    src/pipeline/DiagnosticPipeline.cpp
)

add_executable(volt-steth ${CORE_SOURCES} src/main.cpp)
target_link_libraries(volt-steth PRIVATE ${ALSA_LIBRARIES})

# Suíte de Testes Automatizados
enable_testing()

add_executable(test-pipeline
    src/config/AppConfig.cpp
    src/filters/GroundLoopDetector.cpp
    src/filters/VoltageRippleDetector.cpp
    tests/test_pipeline.cpp
)
target_link_libraries(test-pipeline PRIVATE m)

add_test(NAME PipelineTestSuite COMMAND test-pipeline)
