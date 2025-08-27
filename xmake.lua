add_rules("mode.debug", "mode.release")

set_languages("c++20")
set_warnings("all", "error")

if is_mode("debug") then
    set_policy("build.sanitizer.address", true)
    set_optimize("fast")
    set_warnings("all", "extra", "pedantic")
else
    set_symbols("hidden")
    set_optimize("fastest")
    set_policy("build.optimization.lto", true)
    set_strip("debug")
end

target("corosig")
set_kind("static")
add_includedirs("include", { public = true })
add_files("src/*.cpp")
set_default(true)
target_end()

add_requires("catch2", { configs = { main = true, gmock = true } })

for _, file in ipairs(os.files("test/*.cpp")) do
    local name = "test_" .. path.basename(file)
    target(name)
    set_kind("binary")
    add_deps("corosig")
    add_packages("catch2")
    add_files(file)
    add_tests("default")
end
