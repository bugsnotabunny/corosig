add_rules("mode.debug", "mode.asan", "mode.tsan", "mode.release", "mode.minsizerel")

option("tests")
    set_default(true)
    set_showmenu(true)
    set_description("Build tests")
option_end()

option("examples")
    set_default(true)
    set_showmenu(true)
    set_description("Build examples")
option_end()

option("benchmarks")
    set_default(true)
    set_showmenu(true)
    set_description("Build benchmarks")
option_end()

set_languages("c++20")
set_warnings("all", "extra", "pedantic")

if is_mode("release") then
    set_optimize("fastest")
    add_defines("NDEBUG")
    set_strip("debug")
elseif is_mode("minsizerel") then
    set_optimize("smallest")
    add_defines("NDEBUG")
    set_strip("debug")
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


add_requires("boost 1.90.0", { external = true, configs = { filesystem = false } })


target("corosig")
    set_kind("static")
    add_includedirs("include", { public = true })
    add_files("src/**.cpp")
    add_headerfiles("include/(**.hpp)")
    set_default(true)
    add_packages("boost", { external = true, public = true })

    before_build(function (target)
        if is_mode("asan") then
            target:add("defines", "COROSIG_ASAN_ENABLED=1")
        end
    end)
target_end()


if has_config("tests") or has_config("benchmarks") then
    add_requires("catch2 v3.10.0", { external = true, configs = { main = false, gmock = false } })
end


target("corosig-testing")
    set_enabled(has_config("tests"))
    set_kind("shared")
    add_includedirs("test/lib/include", { public = true })
    add_files("test/lib/src/**.cpp")
    add_headerfiles("test/lib/include/(**.hpp)")
    add_deps("corosig", { public = true })
    add_packages("catch2", { external = true, public = true })
    add_cxflags("-fvisibility=default")
target_end()


for _, file in ipairs(os.files("test/cases/**.cpp")) do
    local rel_path = path.relative(file, "test/cases")
    local name = "test." .. rel_path:gsub("/", "."):sub(1, -5)
    target(name)
        set_enabled(has_config("tests"))
        set_kind("binary")
        add_deps("corosig-testing")
        add_files(file)
        add_tests("default", { runargs = { "--skip-benchmarks" } })
        add_tests("norandord", { runargs = { "--skip-benchmarks", "--order=decl" } })
    target_end()
end


for _, file in ipairs(os.files("benchmark/**.cpp")) do
    local name = "benchmark." .. path.basename(file)
    target(name)
        set_enabled(has_config("benchmarks"))
        add_tests("benchmark", { runargs = {  "--order=decl" } })
        set_kind("binary")
        add_deps("corosig")
        add_packages("catch2", { external = true, public = true })
        add_files(file)
    target_end()
end


for _, file in ipairs(os.files("example/**.cpp")) do
    local name = "example." .. path.basename(file)
    target(name)
        set_enabled(has_config("examples"))
        set_kind("binary")
        add_deps("corosig")
        add_files(file)
    target_end()
end
