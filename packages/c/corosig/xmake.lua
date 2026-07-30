package("corosig")
    set_description("corosig - A signal handling library for C++20")
    set_homepage("https://github.com/bugsnotabunny/corosig")
    set_license("MIT")

    add_urls("https://github.com/bugsnotabunny/corosig.git")

    add_versions("experimental", "master")
    add_versions("v0.1.0", "v0.1.0")

    add_deps("boost", { configs = { filesystem = false } })

    on_install(function (package)
        import("package.tools.xmake").install(package, { tests = false, examples = false, benchmarks = false })
    end)

    on_test(function (package)
        assert(package:has_cxxincludes("corosig/Sighandler.hpp", {configs = {languages = "cxx20"}}))
    end)
