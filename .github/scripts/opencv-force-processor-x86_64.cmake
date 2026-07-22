# Injected via CMAKE_PROJECT_INCLUDE after opencv's project() call. Forces
# CMAKE_SYSTEM_PROCESSOR into the cache so that third-party subdirectories
# (mlas) that read the cache directly see x86_64 instead of the arm64 host
# processor on Apple Silicon CI runners.
set(CMAKE_SYSTEM_PROCESSOR
    x86_64
    CACHE INTERNAL "" FORCE)
