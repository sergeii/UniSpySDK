add_library(usstats
    gbucket.c
    gstats.c
    gbucket.h
    gpersist.h
    gstats.h
)

target_link_libraries(usstats PUBLIC uscommon)

if(RS_BUILD_TESTS)
    add_subdirectory(statstest)
    add_subdirectory(persisttest)
    add_subdirectory(track)

    if (UNISPY_WINDOWS)
        add_subdirectory(gp_stats)
        add_subdirectory(multiTrack)
        add_subdirectory(ladderTrack)
    endif()
endif()
