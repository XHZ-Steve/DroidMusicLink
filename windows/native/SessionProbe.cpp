#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <iostream>
#include <algorithm>
#include <cwctype>
#include <string>

int main(int argc, char** argv) {
    if (argc > 2) return 64;
    const std::string command = argc == 2 ? argv[1] : "";
    if (!command.empty() && command != "--play" && command != "--pause" && command != "--seek-fixture") return 64;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        auto manager = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        int matches = 0;
        std::cout << "Total system sessions: " << manager.GetSessions().Size() << '\n';
        for (auto const& session : manager.GetSessions()) {
            auto identity = std::wstring(session.SourceAppUserModelId());
            auto lower = identity;
            std::transform(lower.begin(), lower.end(), lower.begin(), std::towlower);
            // Do not read or print metadata belonging to any unrelated player.
            if (lower.find(L"droidmedialink") == std::wstring::npos && lower.find(L"link_probe") == std::wstring::npos) continue;
            auto properties = session.TryGetMediaPropertiesAsync().get();
            auto timeline = session.GetTimelineProperties();
            std::wcout << L"MATCH SourceAppUserModelId=" << identity << L"\n"
                       << L"Title=" << properties.Title().c_str() << L"\n"
                       << L"Artist=" << properties.Artist().c_str() << L"\n"
                       << L"PositionMs=" << timeline.Position().count() / 10000 << L"\n"
                       << L"PlaybackStatus=" << static_cast<int>(session.GetPlaybackInfo().PlaybackStatus()) << L"\n";
            ++matches;
            if (!command.empty()) {
                // Never control a real or unrelated player through this test utility.
                if (properties.Title() != L"TEST SESSION") return 3;
                bool accepted = command == "--play" ? session.TryPlayAsync().get()
                    : command == "--pause" ? session.TryPauseAsync().get()
                    : session.TryChangePlaybackPositionAsync(57000LL * 10000).get();
                std::cout << "System control accepted: " << accepted << '\n';
                if (!accepted) return 4;
            }
        }
        std::cout << "Target sessions recognized: " << matches << '\n';
        return matches ? 0 : 1;
    } catch (winrt::hresult_error const& error) {
        std::cerr << "Session probe HRESULT: " << std::hex << error.code().value << '\n';
        return 2;
    }
}
