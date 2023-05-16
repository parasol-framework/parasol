if("${CMAKE_C_COMPILER_ARCHITECTURE_ID}" STREQUAL "")
    set(HOST_ARCH ${CMAKE_SYSTEM_PROCESSOR})
else()
    set(HOST_ARCH ${CMAKE_C_COMPILER_ARCHITECTURE_ID})
endif()

if("${TARGET_PLATFORM}" STREQUAL "")
    if(HOST_ARCH MATCHES x64|x86_64|AMD64|amd64)
        if(CMAKE_SYSTEM_NAME STREQUAL Windows)
            set(TARGET_PLATFORM X86_WIN64)
        else()
            set(TARGET_PLATFORM X86_64)
        endif()
    elseif(HOST_ARCH MATCHES i.*86.*|X86|x86)
        if(MSVC)
            set(TARGET_PLATFORM X86_WIN32)
        else()
            set(TARGET_PLATFORM X86)
        endif()

        if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
            set(TARGET_PLATFORM X86_DARWIN)
        elseif(CMAKE_SYSTEM_NAME MATCHES FreeBSD|OpenBSD)
            set(TARGET_PLATFORM X86_FREEBSD)
        endif()
    elseif(HOST_ARCH MATCHES aarch64|ARM64|arm64)
        if(MSVC)
            set(TARGET_PLATFORM ARM_WIN64)
        else()
            set(TARGET_PLATFORM AARCH64)
        endif()
    elseif(HOST_ARCH MATCHES arm.*|ARM.*)
        if(MSVC)
            set(TARGET_PLATFORM ARM_WIN32)
        else()
            set(TARGET_PLATFORM ARM)
        endif()
    else()
        message(FATAL_ERROR "Unknown host.")
    endif()
endif()

message(STATUS "Building for TARGET_PLATFORM: ${TARGET_PLATFORM}")