add_rules("mode.debug", "mode.coverage", "mode.release", "mode.minsizerel")
add_rules("plugin.compile_commands.autoupdate", { lsp = "clangd" })

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

option("asan")
    set_default(false)
    set_showmenu(true)
    set_description("Build with asan, ubsan and lsan")
option_end()

option("tsan")
    set_default(false)
    set_showmenu(true)
    set_description("Build with tsan")
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
    add_cxflags("-g3", "-gz", "-Og")
    add_ldflags("-g3", "-gz", "-Og")
end

if has_config("asan") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
    set_policy("build.sanitizer.leak", true)
end

if has_config("tsan") then
    set_policy("build.sanitizer.thread", true)
end

add_requires("boost 1.92.0", { external = true, configs = { filesystem = false } })

target("corosig")
    set_kind("static")
    add_includedirs("include", { public = true })
    add_files("src/**.cpp")
    add_headerfiles("include/(**.hpp)")
    set_default(true)
    add_packages("boost", { external = true, public = true })

    before_build(function (target)
        if target:policy("build.sanitizer.address") then
            target:add("defines", "COROSIG_ASAN_ENABLED=1", { private = true })
        end
    end)
target_end()


if has_config("tests") or has_config("benchmarks") then
    add_requires("catch2 3.16.0")
end


task("tidy-check")
    set_menu {
        usage = "xmake tidy-check",
        description = "Run clang-tidy in parallel and apply fixes",
        options = {
            { nil, "fix", "k", nil, "Set the number of parallel jobs" },
            { 'j', "jobs", "kv", nil, "Set the number of parallel jobs" },
        }
    }
    on_run(function ()
        import("core.project.config")
        import("core.project.project")
        import("core.base.option")
        import("core.base.task")
        import("utils.progress")
        import("async.runjobs")

        config.load()

        local fixdir = path.join(config.builddir(), "clang-tidy-fixes")
        task.run("project", {kind = "compile_commands", lsp = "clangd", outputdir = fixdir})

        os.tryrm(fixdir)
        os.mkdir(fixdir)

        local files = {}
        for _, target in pairs(project.targets()) do
            for _, sourcefile in ipairs(target:headerfiles()) do
                table.insert(files, sourcefile)
            end
            for _, sourcefile in ipairs(target:sourcefiles()) do
                table.insert(files, sourcefile)
            end
        end

        local jobs = tonumber(option.get("jobs")) or os.default_njob() -- or os.cpuinfo().ncpu
        print("Running clang-tidy on %d files using %d jobs...", #files, jobs)
        runjobs("clang-tidy", function (index, total, opt)
            local file = files[index]
            progress.show(opt.progress, "${color.build.progress}clang-tidy${clear} %s", file)

            if option.get("fix") then
                local yamlfile = path.join(fixdir, string.format("fix_%d.yaml", index))
                local ok = os.execv("clang-tidy", { "-p", fixdir, "--export-fixes=" .. yamlfile, file }, { try = true })
            else
                os.execv("clang-tidy", { "-p", fixdir, file })
            end
        end, { total = #files, comax = jobs })

        if option.get("fix") then
            print("Applying fixes...")
            os.vrunv("clang-apply-replacements", { "--remove-change-desc-files", fixdir })
        end
        os.rm(fixdir)
    end)
task_end()

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
        add_tests("default", { runargs = { "--skip-benchmarks" }, timeout = 120 })
        add_tests("norandord", { runargs = { "--skip-benchmarks", "--order=decl" }, timeout = 120 })
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
