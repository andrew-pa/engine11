add_executable(egg-demo
    src/main.cpp)
target_compile_features(egg-demo PUBLIC cxx_std_20)
target_link_libraries(egg-demo egg egg-forward-renderer)
