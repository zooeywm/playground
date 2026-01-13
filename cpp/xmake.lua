set_project("mixed_project")
set_version("0.1.0")

add_rules("mode.debug", "mode.release")

target("fixed_stack")
    set_kind("binary")
    set_languages("c++20")
    set_plat("linux")
    set_arch("x86_64")
    add_files("src/shm_stack/*.cpp")

target("dxgi_pointer_monitor")
    set_kind("binary")

    set_plat("mingw")
    set_arch("x86_64")

    add_rules("qt.quickapp", "qt.moc")

    add_files("src/dxgi_pointer_monitor/*.cpp")
    add_files("src/dxgi_pointer_monitor/*.h")
    add_syslinks("d3d11")
