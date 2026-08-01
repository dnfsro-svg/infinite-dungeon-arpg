add_library(arpg_persistence STATIC
    checkpoint_codec.cpp
    room_progress_codec.cpp
    save_commit_worker.cpp
    save_paths.cpp
    save_slot.cpp
    save_transaction.cpp
    save_recovery.cpp
    save_store.cpp)

target_include_directories(arpg_persistence PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_persistence PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(arpg_persistence PUBLIC /EHsc)
endif()
arpg_enable_project_warnings(arpg_persistence)
target_link_libraries(arpg_persistence PUBLIC arpg_checkpoint)
