add_rules("mode.debug", "mode.release")

target("fixed_stack")
set_languages("c++20")
add_files("src/shm_stack/*.cpp")
