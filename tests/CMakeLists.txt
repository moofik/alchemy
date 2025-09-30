# tests/CMakeLists.txt

# Исполняемый файл для юнит-тестов по SPSC
add_executable(TestsSpscRing
        TestSpscRing.cpp
)

# Нужны хедеры из src/ (чтобы видеть utils/SpscRing.h)
target_include_directories(TestsSpscRing PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Линкуем готовый main от Catch2 v3
target_link_libraries(TestsSpscRing PRIVATE Catch2::Catch2WithMain)

# Регистрируем тест для ctest
add_test(NAME SpscRing COMMAND TestsSpscRing)
