#include "gameview.hpp"

#include <fmt/format.h>

#include "dialog.hpp"
#include "file.hpp"
#include "imgui.hpp"
extern "C"
{
#include "style.h"
}

namespace
{
constexpr unsigned GameViewWidth = VITA_WIDTH * 0.8;
constexpr unsigned GameViewHeight = VITA_HEIGHT * 0.8;
}

GameView::GameView(
        const Config* config,
        Downloader* downloader,
        DbItem* item,
        std::optional<CompPackDatabase::Item> base_comppack,
        std::optional<CompPackDatabase::Item> patch_comppack)
    : _config(config)
    , _downloader(downloader)
    , _item(item)
    , _base_comppack(base_comppack)
    , _patch_comppack(patch_comppack)
    , _patch_info_fetcher(item->titleid)
    , _image_fetcher(item)
{
    refresh();
}

void GameView::render()
{
    ImGui::SetNextWindowPos(
            ImVec2((VITA_WIDTH - GameViewWidth) / 2,
                   (VITA_HEIGHT - GameViewHeight) / 2));
    ImGui::SetNextWindowSize(ImVec2(GameViewWidth, GameViewHeight), 0);

    ImGui::Begin(
            fmt::format("{} ({})###gameview", _item->name, _item->titleid)
                    .c_str(),
            nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoInputs);

    ImGui::PushTextWrapPos(_image_fetcher.get_texture() == nullptr ?
        0.f : GameViewWidth - 300.f);
    ImGui::Text(fmt::format("Версия ПО: {}", pkgi_get_system_version())
                        .c_str());
    ImGui::Text(
            fmt::format(
                    "Требуемая версия ПО: {}", get_min_system_version())
                    .c_str());

    ImGui::Text(" ");

    ImGui::Text(fmt::format(
                        "Установленная версия игры: {}",
                        _game_version.empty() ? "Не установлен" : _game_version)
                        .c_str());
    if (_comppack_versions.present && _comppack_versions.base.empty() &&
        _comppack_versions.patch.empty())
    {
        ImGui::Text("Установленый пакет совместимости: Неизвестная версия");
    }
    else
    {
        ImGui::Text(fmt::format(
                            "Установленный базовый пакет совместимости: {}",
                            _comppack_versions.base.empty() ? "Нет" : "Да")
                            .c_str());
        ImGui::Text(fmt::format(
                            "Установленная версия пакета совместимости с патчами: {}",
                            _comppack_versions.patch.empty()
                                    ? "нету"
                                    : _comppack_versions.patch)
                            .c_str());
    }

    ImGui::Text(" ");

    printDiagnostic();

    ImGui::Text(" ");

    ImGui::PopTextWrapPos();

    if (_patch_info_fetcher.get_status() == PatchInfoFetcher::Status::Found)
    {
        if (ImGui::Button("Установить игру и патч###installgame"))
            start_download_package();
    }
    else
    {
        if (ImGui::Button("Установить игру###installgame"))
            start_download_package();
    }

    if (_base_comppack)
    {
        if (!_downloader->is_in_queue(CompPackBase, _item->titleid))
        {
            if (ImGui::Button("Установить базовый пакет совместимости"
                              "pack###installbasecomppack"))
                start_download_comppack(false);
        }
        else
        {
            if (ImGui::Button("Отменить установку базового пакета совместимости"
                              "installation###installbasecomppack"))
                cancel_download_comppacks(false);
        }
    }
    if (_patch_comppack)
    {
        if (!_downloader->is_in_queue(CompPackPatch, _item->titleid))
        {
            if (ImGui::Button(fmt::format(
                                      "Install compatibility pack "
                                      "{}###installpatchcommppack",
                                      _patch_comppack->app_version)
                                      .c_str()))
                start_download_comppack(true);
        }
        else
        {
            if (ImGui::Button("Cancel patch compatibility pack "
                              "installation###installpatchcommppack"))
                cancel_download_comppacks(true);
        }
    }

    auto tex = _image_fetcher.get_texture();
    // Display game image
    if (tex != nullptr)
    {
        int tex_w = vita2d_texture_get_width(tex);
        int tex_h = vita2d_texture_get_height(tex);
        float tex_x = ImGui::GetWindowContentRegionMax().x - tex_w;
        float tex_y = ImGui::GetWindowContentRegionMin().y;
        ImGui::SetCursorPos(ImVec2(tex_x, tex_y));
        ImGui::Image(tex, ImVec2(tex_w, tex_h));
    }

    ImGui::End();
}

static const auto Red = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
static const auto Yellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
static const auto Green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

void GameView::printDiagnostic()
{
    bool ok = true;
    auto const printError = [&](auto const& str) {
        ok = false;
        ImGui::TextColored(Red, str);
    };

    auto const systemVersion = pkgi_get_system_version();
    auto const minSystemVersion = get_min_system_version();

    ImGui::Text("Diagnostic:");

    if (systemVersion < minSystemVersion)
    {
        if (!_comppack_versions.present)
        {
            if (_refood_present)
                ImGui::Text(
                        "- Эта игра будет работать благодаря reF00D");
            else if (_0syscall6_present)
                ImGui::Text(
                        "- Эта игра будет работать благодаря 0syscall6");
            else
                printError(
                        "- Ваша версия ПО слишком стара, чтобы играть в эту игру, вы... "
                        "нужно установить reF00D или 0syscall6");
        }
    }
    else
    {
        ImGui::Text(
                "- Ваша версия ПО достаточно свежая.");
    }

    if (_comppack_versions.present && _comppack_versions.base.empty() &&
        _comppack_versions.patch.empty())
    {
        ImGui::TextColored(
                Yellow,
                "- Пакет совместимости установлен, но не с помощью PKGj, пожалуйста. "
                "убедитесь, что она совпадает с установленной версией или переустановите ее. "
                "с помощью PKGj");
        ok = false;
    }

    if (_comppack_versions.base.empty() && !_comppack_versions.patch.empty())
        printError(
                "- Вы установили пакет обновления совместимости без "
                "установки базовый пакет, сначала установите базовый пакет, и "
                "переустановите  обновление пакета совместимости.");

    std::string comppack_version;
    if (!_comppack_versions.patch.empty())
        comppack_version = _comppack_versions.patch;
    else if (!_comppack_versions.base.empty())
        comppack_version = _comppack_versions.base;

    if (_item->presence == PresenceInstalled && !comppack_version.empty() &&
        comppack_version < _game_version)
        printError(
                "- The version of the game does not match the installed "
                "compatibility pack. If you have updated the game, also "
                "install the update compatibility pack.");

    if (_item->presence == PresenceInstalled &&
        comppack_version > _game_version)
        printError(
                "- The version of the game does not match the installed "
                "compatibility pack. Downgrade to the base compatibility "
                "pack or update the game through the Live Area.");

    if (_item->presence != PresenceInstalled)
    {
        ImGui::Text("- Игра не установлена");
        ok = false;
    }

    if (ok)
        ImGui::TextColored(Green, "All green");
}

std::string GameView::get_min_system_version()
{
    auto const patchInfo = _patch_info_fetcher.get_patch_info();
    if (patchInfo)
        return patchInfo->fw_version;
    else
        return _item->fw_version;
}

void GameView::refresh()
{
    LOGF("refreshing gameview");
    _refood_present = pkgi_is_module_present("ref00d");
    _0syscall6_present = pkgi_is_module_present("0syscall6");
    _game_version = pkgi_get_game_version(_item->partition, _item->titleid);
    _comppack_versions = pkgi_get_comppack_versions(_item->partition, _item->titleid);
}

void GameView::start_download_package()
{
    if (_item->presence == PresenceInstalled)
    {
        LOGF("[{}] {} - already installed", _item->titleid, _item->name);
        pkgi_dialog_error("Уже установлено");
        return;
    }

    pkgi_start_download(*_downloader, *_item);

    _item->presence = PresenceUnknown;
}

void GameView::cancel_download_package()
{
    _downloader->remove_from_queue(Game, _item->content);
    _item->presence = PresenceUnknown;
}

void GameView::start_download_comppack(bool patch)
{
    const auto& entry = patch ? _patch_comppack : _base_comppack;

    _downloader->add(DownloadItem{patch ? CompPackPatch : CompPackBase,
                                  _item->name,
                                  _item->titleid,
                                  _config->comppack_url + entry->path,
                                  std::vector<uint8_t>{},
                                  std::vector<uint8_t>{},
                                  false,
                                  _config->install_psv_location,
                                  _config->install_psp_game_path,
                                  _config->install_psp_iso_path,
                                  _config->install_psp_psx_path,
                                  entry->app_version});
}

void GameView::cancel_download_comppacks(bool patch)
{
    _downloader->remove_from_queue(
            patch ? CompPackPatch : CompPackBase, _item->titleid);
}
