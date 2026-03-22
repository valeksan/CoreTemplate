# CoreTemplate Tests

## Structure

- `core_tests.cpp` - QtTest test suite for the `Core` API.
- `CMakeLists.txt` - CMake build file (Qt5/Qt6).
- `core_tests.pro` - qmake project file.

## Run with CMake

```powershell
cmake -S tests -B tests/build
cmake --build tests/build --config Debug
ctest --test-dir tests/build --output-on-failure -C Debug
```

## Run with qmake

```powershell
cd tests
qmake core_tests.pro
mingw32-make
./CoreTemplateTests.exe
```
