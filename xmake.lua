add_rules("mode.debug", "mode.asan", "mode.tsan", "mode.release")

set_languages("c++20")
set_warnings("all", "error")

if is_mode("release") then
    set_symbols("hidden")
    set_optimize("fastest")
    set_policy("build.optimization.lto", true)
    set_strip("debug")
else
    set_optimize("fast")
    set_warnings("all", "extra", "pedantic")
end

if is_mode("asan") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
    set_policy("build.sanitizer.leak", true)
end

if is_mode("tsan") then
    set_policy("build.sanitizer.thread", true)
end


add_requires("boost", {
    configs = {
        exception = true, -- implicitly required by outcome
        container = true,
    }
})

target("corosig")
set_kind("static")
add_includedirs("include", { public = true })
add_files("src/**.cpp")
set_default(true)
add_packages("boost", { public = true })
target_end()


add_requires("catch2 v2.13.10", { configs = { main = true, gmock = true } })

target("corosig-testing")
set_kind("static")
add_includedirs("test/lib/include", { public = true })
add_files("test/lib/src/**.cpp")
add_deps("corosig", { public = true })
add_packages("catch2", { public = true })
target_end()


for _, file in ipairs(os.files("test/cases/**.cpp")) do
    local name = "test_" .. path.basename(file)
    target(name)
    set_kind("binary")
    add_deps("corosig-testing")
    add_files(file)
    add_tests("default")
end
