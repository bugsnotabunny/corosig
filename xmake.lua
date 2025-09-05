add_rules("mode.debug", "mode.release")

set_languages("c++20")
set_warnings("all", "error")

if is_mode("debug") then
    set_policy("build.sanitizer.thread", true)
    set_optimize("fast")
    set_warnings("all", "extra", "pedantic")
else
    set_symbols("hidden")
    set_optimize("fastest")
    set_policy("build.optimization.lto", true)
    set_strip("debug")
end

add_requires("boost", {
    configs = { exception = true } -- implicitly required by outcome
})

add_requires("ace", {
    configs = { ssl = false }
})

target("corosig")
set_kind("static")
add_includedirs("include", { public = true })
add_files("src/**.cpp")
set_default(true)
add_packages("boost", { public = true })
add_packages("ace", { public = true })
target_end()

add_requires("catch2", { configs = { main = true, gmock = true } })


target("corosig-testing")
set_kind("static")
add_includedirs("test/lib/include", { public = true })
add_files("test/lib/src/**.cpp")
add_deps("corosig", { public = true })
add_packages("catch2", { public = true })
target_end()


for _, file in ipairs(os.files("test/cases/*.cpp")) do
    local name = "test_" .. path.basename(file)
    target(name)
    set_kind("binary")
    add_deps("corosig-testing")
    add_packages("catch2")
    add_files(file)
    add_tests("default")
end
