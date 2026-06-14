
if (MINGW)
    #Required for static SDL linking
    SET(EXE_LINK_LIBS_POST ${EXE_LINK_LIBS_POST} "winmm;imm32;setupapi;version;ws2_32;iphlpapi")
endif ()

find_package(SDL3)
find_package(SDL3_image)
if (SDL3_FOUND AND SDL3_image_FOUND)
    message("Using SDL version: SDL3")
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)
    FIND_PACKAGE(SDL3_image REQUIRED)
    FIND_PACKAGE(SDL3_mixer REQUIRED)
    FIND_PACKAGE(SDL3_net REQUIRED)

    add_definitions(-DPERIMETER_SDL3)
    set(PERIMETER_SDL_LIBRARY ${SDL3_LIBRARY})
    set(PERIMETER_SDL_EXTRA_LIBRARIES ${SDL3_IMAGE_LIBRARY} ${SDL3_MIXER_LIBRARY} ${SDL3_NET_LIBRARY})
else ()
    message("Using SDL version: SDL2")
    if (MSVC_CL_BUILD)
        #Specifics to MSVC+VCPKG
        FIND_PACKAGE(SDL2 CONFIG REQUIRED)
        SET(SDL2_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/SDL2")
        SET(SDL2_LIBRARY SDL2::SDL2)
        SET(SDL2MAIN_LIBRARY SDL2::SDL2main)
        FIND_PACKAGE(SDL2_image CONFIG REQUIRED)
        SET(SDL2_IMAGE_LIBRARY $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>)
        FIND_PACKAGE(SDL2_mixer CONFIG REQUIRED)
        SET(SDL2_MIXER_LIBRARY $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>)
        FIND_PACKAGE(SDL2_net CONFIG REQUIRED)
        SET(SDL2_NET_LIBRARY $<IF:$<TARGET_EXISTS:SDL2_net::SDL2_net>,SDL2_net::SDL2_net,SDL2_net::SDL2_net-static>)
    else ()
        FIND_PACKAGE(SDL2 REQUIRED)
        FIND_PACKAGE(SDL2-image REQUIRED)
        FIND_PACKAGE(SDL2-mixer REQUIRED)
        FIND_PACKAGE(SDL2-net REQUIRED)
    endif ()
    
    include_directories(SYSTEM
            ${SDL2_INCLUDE_DIR}
            ${SDL2_IMAGE_INCLUDE_DIR}
            ${SDL2_MIXER_INCLUDE_DIR}
            ${SDL2_NET_INCLUDE_DIR}
    )

    set(PERIMETER_SDL_LIBRARY ${SDL2_LIBRARY})
    set(PERIMETER_SDL_EXTRA_LIBRARIES ${SDL2_IMAGE_LIBRARY} ${SDL2_MIXER_LIBRARY} ${SDL2_NET_LIBRARY})
    set(PERIMETER_SDL_MAIN_LIBRARY ${SDL2MAIN_LIBRARY})
endif ()
