add_executable(wiretone_sender src/main.cpp)

target_link_libraries(wiretone_sender PRIVATE WireTone::Core)
target_compile_features(wiretone_sender PRIVATE cxx_std_20)
wiretone_enable_warnings(wiretone_sender)
