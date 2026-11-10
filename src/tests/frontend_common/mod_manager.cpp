// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "common/fs/path_util.h"
#include "frontend_common/mod_manager.h"

namespace {
namespace fs = std::filesystem;

class ModInstallFixture {
public:
    ModInstallFixture() {
        root = fs::temp_directory_path() /
               ("fritchii-mod-install-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        REQUIRE(fs::create_directory(root));
        REQUIRE(fs::create_directory(root / "load"));
        REQUIRE(fs::create_directory(Installed().parent_path()));
        Common::FS::SetEdenPath(Common::FS::EdenPath::LoadDir, root / "load");
        REQUIRE(Common::FS::GetEdenPath(Common::FS::EdenPath::LoadDir) == root / "load");
    }

    ~ModInstallFixture() {
        Common::FS::SetAppDirectory({});
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    fs::path Installed() const {
        return root / "load" / "0100000000000000" / "SampleMod";
    }

    void WriteMod(const fs::path& path, const std::string& text) const {
        REQUIRE(fs::create_directories(path / "romfs"));
        REQUIRE(std::ofstream(path / "romfs" / "payload").write(text.data(), text.size()).good());
    }

    std::string ReadPayload(const fs::path& path) const {
        std::ifstream file(path / "romfs" / "payload");
        std::string text;
        std::getline(file, text);
        return text;
    }

    fs::path root;
};

constexpr u64 ProgramId = 0x0100000000000000ULL;
} // namespace

TEST_CASE("Reinstalling a mod from its installed folder preserves it", "[frontend_common][mod_manager]") {
    const bool copy = GENERATE(true, false);
    const bool dotted_path = GENERATE(false, true);
    ModInstallFixture fixture;
    const auto installed = fixture.Installed();
    fixture.WriteMod(installed, "keep this mod");
    REQUIRE(std::ofstream(installed.parent_path() / "keep").put('x').good());
    const auto selected = dotted_path ? installed.parent_path() / "." / installed.filename()
                                      : installed;
    const auto mods = FrontendCommon::GetModFolder(selected.string());
    REQUIRE(mods.size() == 1);
    REQUIRE(fs::equivalent(mods.front(), installed));

    CHECK(FrontendCommon::InstallMod(mods.front(), ProgramId, copy) == FrontendCommon::Success);
    CHECK(fixture.ReadPayload(installed) == "keep this mod");
    CHECK(fs::exists(installed.parent_path() / "keep"));
}

TEST_CASE("Installing a separate mod still copies or moves and replaces old files",
          "[frontend_common][mod_manager]") {
    const bool copy = GENERATE(true, false);
    const bool replace = GENERATE(false, true);
    ModInstallFixture fixture;
    const auto source = fixture.root / "download" / "SampleMod";
    const auto installed = fixture.Installed();
    fixture.WriteMod(source, "new mod");
    if (replace) {
        fixture.WriteMod(installed, "old mod");
        REQUIRE(std::ofstream(installed / "stale").put('x').good());
    }

    REQUIRE(FrontendCommon::InstallMod(source, ProgramId, copy) == FrontendCommon::Success);
    CHECK(fixture.ReadPayload(installed) == "new mod");
    CHECK_FALSE(fs::exists(installed / "stale"));
    CHECK(fs::exists(source) == copy);
}

#ifdef _WIN32
TEST_CASE("Reinstalling a mod with different path casing preserves it",
          "[frontend_common][mod_manager]") {
    ModInstallFixture fixture;
    const auto installed = fixture.Installed();
    fixture.WriteMod(installed, "keep this mod");
    const auto selected = installed.parent_path() / "samplemod";
    REQUIRE(fs::equivalent(selected, installed));

    CHECK(FrontendCommon::InstallMod(selected, ProgramId) == FrontendCommon::Success);
    CHECK(fixture.ReadPayload(installed) == "keep this mod");
}
#endif
