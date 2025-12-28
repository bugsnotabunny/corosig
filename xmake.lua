add_rules("mode.debug", "mode.asan", "mode.tsan", "mode.release")

set_languages("c++20")
set_warnings("all", "extra", "pedantic")

if is_mode("fast") then
    set_optimize("fastest")
    add_defines("NDEBUG")
    set_strip("debug")
    set_policy("build.optimization.lto", true)
elseif is_mode("small") then
    set_optimize("smallest")
    add_defines("NDEBUG")
    set_strip("debug")
    set_policy("build.optimization.lto", true)
else
    set_optimize("fast")
end

if is_mode("asan") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
    set_policy("build.sanitizer.leak", true)
elseif is_mode("tsan") then
    set_policy("build.sanitizer.thread", true)
end


add_requires("boost 1.86.0", { configs = { filesystem = false } })


target("corosig")
    set_kind("shared")
    add_includedirs("include", { public = true })
    add_files("src/**.cpp")
    set_default(true)
    add_packages("boost", { public = true })
target_end()


add_requires("catch2 v3.10.0", { configs = { lto = false, main = false, gmock = false } })


target("corosig-testing")
    set_kind("shared")
    add_includedirs("test/lib/include", { public = true })
    add_files("test/lib/src/**.cpp")
    add_deps("corosig", { public = true })
    add_packages("catch2", { public = true })
target_end()


for _, file in ipairs(os.files("test/cases/**.cpp")) do
    local name = "Test" .. path.basename(file)
    target(name)
        set_kind("binary")
        add_deps("corosig-testing")
        add_files(file)
    add_tests("default")
end


for _, file in ipairs(os.files("example/**.cpp")) do
    local name = "Example" .. path.basename(file)
    target(name)
        set_kind("binary")
        add_deps("corosig")
        add_files(file)
end
