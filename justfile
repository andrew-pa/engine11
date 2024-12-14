out_dir := ".build"

# Choose a task to run.
default:
    @just --choose

# Delete generated outputs.
clean:
    rm -rf {{out_dir}}

# Configure build system (run CMake).
config:
    cmake -S . -B {{out_dir}} -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug -GNinja
    # Create symlink for compile_commands.json so editors can find it.
    ln -svr {{out_dir}} ./compile_commands.json || true

# Run code formatter.
fmt:
    find include/ src/ -type f -name "*.cpp" -or -name "*.h" | xargs clang-format -style=file -i

# Build everything.
build:
    ninja -C {{out_dir}}
