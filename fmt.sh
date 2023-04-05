#!/bin/sh
find include/ src/ -type f -name "*.cpp" -or -name "*.h" | xargs clang-format -style=file -i
