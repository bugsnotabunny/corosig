set_languages("c++20")
set_warnings("all", "extra", "pedantic")

local corosig_version_option = "corosig-version"

option(corosig_version_option)
    set_showmenu(true)
    set_description("Corosig version to test with")

    on_check(function ()
        local version = get_config(corosig_version_option)
        if not version or version == "" then
            raise(corosig_version_option .. " option is required to be set")
        end
    end)
option_end()

local version = get_config(corosig_version_option)
if not version then
    version = ""
end

local branch = version
if version == "experimental" then
    branch = "master"
end

add_repositories("corosig-repo git@github.com:bugsnotabunny/corosig.git " .. branch)
add_requires("corosig " .. version, { external = true })

-- override boost settings and version, if needed
add_requireconfs("corosig.boost", { override = true, version = "1.90.0" })

target("test")
    set_kind("binary")
    add_packages("corosig")
    add_files("main.cpp")
target_end()
