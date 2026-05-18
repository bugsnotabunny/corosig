#ifndef COROSIG_TESTING_FILE_HELPERS_HPP
#define COROSIG_TESTING_FILE_HELPERS_HPP

#include <filesystem>

namespace corosig::testing {

std::filesystem::path tmp_file();
void write_file(std::string_view path, std::string_view content);

} // namespace corosig::testing

#endif
