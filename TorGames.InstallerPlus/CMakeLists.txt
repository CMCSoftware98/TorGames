cmake_minimum_required(VERSION 3.20)
project(TorGames.InstallerPlus VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific settings for MINIMAL size
if(MSVC)
    # Use static runtime to avoid DLL dependencies
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # Aggressive size optimization
    add_compile_options(
        /O1             # Optimize for size
        /Os             # Favor small code
        /GS-            # Disable buffer security checks
        /Gy             # Enable function-level linking
        /GL             # Whole program optimization
        /GR-            # Disable RTTI
        /EHs-c-         # Disable exceptions (use /EHsc if you need them)
        /Zc:threadSafeInit-  # Disable thread-safe static init
    )

    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )

    add_link_options(
        /LTCG           # Link-time code generation
        /OPT:REF        # Remove unreferenced functions
        /OPT:ICF        # Identical COMDAT folding
        /INCREMENTAL:NO
        /NODEFAULTLIB:libcmt  # May need adjustment
    )
endif()

# Create the executable (NO gRPC, pure Win32)
add_executable(TorGames.InstallerPlus
    main.cpp
)

# Link only essential Windows libraries
target_link_libraries(TorGames.InstallerPlus PRIVATE
    kernel32
    ws2_32        # Winsock
    winhttp       # HTTP client
    wbemuuid      # WMI
    ole32
    oleaut32
    iphlpapi      # Network info
    bcrypt        # SHA256
    shlwapi
)

# Set output name
set_target_properties(TorGames.InstallerPlus PROPERTIES
    OUTPUT_NAME "TorGames.InstallerPlus"
)

# Post-build: Strip and optionally UPX compress
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Find UPX for compression (optional)
    find_program(UPX_EXECUTABLE upx)
    if(UPX_EXECUTABLE)
        add_custom_command(TARGET TorGames.InstallerPlus POST_BUILD
            COMMAND ${UPX_EXECUTABLE} --best --lzma "$<TARGET_FILE:TorGames.InstallerPlus>"
            COMMENT "Compressing with UPX..."
        )
    endif()
endif()
