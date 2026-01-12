add_rules("mode.debug", "mode.release")

target("fixed_stack")
add_files("src/fixed_stack.cpp")

target("test")
add_files("src/test.cpp")
