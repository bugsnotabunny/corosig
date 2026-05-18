#ifndef COROSIG_TESTING_FILE_HELPERS_HPP
#define COROSIG_TESTING_FILE_HELPERS_HPP

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <random>

namespace corosig::testing {

inline std::filesystem::path tmp_file() {
  auto tmp_dir = std::filesystem::temp_directory_path();
  std::array<char, 16> filename{};
  do {
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<char> distr{'a', 'z'};
    auto rand_letter = [&] { return distr(gen); };
    std::ranges::generate_n(filename.begin(), std::size(filename) - 1, rand_letter);
  } while (std::filesystem::exists(tmp_dir / std::string_view{filename.data(), filename.size()}));
  return tmp_dir / std::string_view{filename.data(), filename.size()};
}

inline void write_file(std::string_view path, std::string_view content) {
  std::ofstream ofs(std::string{path});
  ofs << content;
}

} // namespace corosig::testing

#endif
