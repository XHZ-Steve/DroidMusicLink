#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <roapi.h>
#include <shcore.h>
#include <shlwapi.h>
#include <systemmediatransportcontrolsinterop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Data.Json.h>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <cmath>
#include <vector>
#include <string>

using namespace winrt;
using namespace winrt::Windows::Media;
using namespace winrt::Windows::Data::Json;
using Callback = void(__cdecl*)(const char*, double);
static SystemMediaTransportControls controls{nullptr};
static event_token buttonToken{}, positionToken{};
static std::atomic<Callback> callback{nullptr};
static std::mutex stateMutex;
static hstring lastArtworkPath,lastArtworkRevision;

static winrt::Windows::Storage::Streams::RandomAccessStreamReference artworkSnapshot(hstring const& artwork){
    // StorageFile async .get() is not safe on Qt's STA UI thread. Publish an
    // owned memory snapshot so SMTC also cannot hold/cache our mutable file.
    std::wstring path(artwork);std::replace(path.begin(),path.end(),L'/',L'\\');
    HANDLE raw=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(raw==INVALID_HANDLE_VALUE)throw_last_error();
    handle file{raw};LARGE_INTEGER size{};check_bool(GetFileSizeEx(file.get(),&size));
    if(size.QuadPart<=0||size.QuadPart>2*1024*1024)throw_hresult(E_INVALIDARG);
    std::vector<uint8_t> bytes(static_cast<size_t>(size.QuadPart));DWORD read=0;
    check_bool(ReadFile(file.get(),bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr));
    if(read!=bytes.size())throw_hresult(HRESULT_FROM_WIN32(ERROR_READ_FAULT));
    com_ptr<IStream> memory;memory.attach(SHCreateMemStream(bytes.data(),static_cast<UINT>(bytes.size())));
    if(!memory)throw_hresult(E_OUTOFMEMORY);
    winrt::Windows::Storage::Streams::IRandomAccessStream stream{nullptr};
    check_hresult(CreateRandomAccessStreamOverStream(memory.get(),BSOS_DEFAULT,guid_of<decltype(stream)>(),put_abi(stream)));
    return winrt::Windows::Storage::Streams::RandomAccessStreamReference::CreateFromStream(stream);
}

extern "C" __declspec(dllexport) void __cdecl dml_smtc_shutdown() {
    std::scoped_lock lock(stateMutex);
    callback.store(nullptr);
    lastArtworkPath={};lastArtworkRevision={};
    if (!controls) return;
    try {
        controls.ButtonPressed(buttonToken);
        controls.PlaybackPositionChangeRequested(positionToken);
        controls.PlaybackStatus(MediaPlaybackStatus::Closed);
        controls.IsEnabled(false);
    } catch (...) {}
    controls = nullptr;
}

extern "C" __declspec(dllexport) int __cdecl dml_smtc_init(HWND window, Callback handler) {
    dml_smtc_shutdown();
    try {
        // Qt initializes COM on its UI thread. A pre-existing STA is valid here.
        struct Apartment {
            HRESULT result = RoInitialize(RO_INIT_MULTITHREADED);
            ~Apartment() { if (SUCCEEDED(result)) RoUninitialize(); }
        };
        thread_local Apartment apartment;
        if (FAILED(apartment.result) && apartment.result != RPC_E_CHANGED_MODE) return static_cast<int>(apartment.result);
        auto interop = get_activation_factory<SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
        check_hresult(interop->GetForWindow(window, guid_of<SystemMediaTransportControls>(), put_abi(controls)));
        callback.store(handler);
        buttonToken = controls.ButtonPressed([](auto const&, auto const& args) {
            auto cb = callback.load();
            if (!cb) return;
            switch (args.Button()) {
            case SystemMediaTransportControlsButton::Play: cb("play", 0); break;
            case SystemMediaTransportControlsButton::Pause: cb("pause", 0); break;
            case SystemMediaTransportControlsButton::Next: cb("next", 0); break;
            case SystemMediaTransportControlsButton::Previous: cb("previous", 0); break;
            default: break;
            }
        });
        positionToken = controls.PlaybackPositionChangeRequested([](auto const&, auto const& args) {
            if (auto cb = callback.load()) cb("seek", args.RequestedPlaybackPosition().count() / 10000.0);
        });
        controls.IsEnabled(false);
        return 1;
    } catch (...) { controls = nullptr; return static_cast<int>(to_hresult()); }
}

extern "C" __declspec(dllexport) int __cdecl dml_smtc_update(const char* json) {
    std::scoped_lock lock(stateMutex);
    if (!controls || !json) return -1;
    try {
        auto data = JsonObject::Parse(to_hstring(json));
        bool online = data.GetNamedBoolean(L"online", true);
        controls.IsEnabled(online);
        if (!online) { controls.PlaybackStatus(MediaPlaybackStatus::Closed); return 0; }
        auto commands = data.GetNamedArray(L"commands", JsonArray{});
        auto supports = [&](wchar_t const* command) {
            for (auto const& value : commands) if (value.ValueType() == JsonValueType::String && value.GetString() == command) return true;
            return false;
        };
        controls.IsPlayEnabled(supports(L"play") || supports(L"toggle"));
        controls.IsPauseEnabled(supports(L"pause") || supports(L"toggle"));
        controls.IsNextEnabled(supports(L"next"));
        controls.IsPreviousEnabled(supports(L"previous"));
        auto updater = controls.DisplayUpdater();
        updater.Type(MediaPlaybackType::Music);
        auto music = updater.MusicProperties();
        music.Title(data.GetNamedString(L"title", L""));
        music.Artist(data.GetNamedString(L"artist", L""));
        music.AlbumTitle(data.GetNamedString(L"album", L""));
        auto artwork = data.GetNamedString(L"artwork", L"");
        auto revision=data.GetNamedString(L"artworkRevision",L"");int artworkStatus=0;
        if (!artwork.empty()&&(revision.empty()||artwork!=lastArtworkPath||revision!=lastArtworkRevision)) {
            try {
                updater.Thumbnail(artworkSnapshot(artwork));
                lastArtworkPath=artwork;lastArtworkRevision=revision;
            } catch (...) {
                artworkStatus=static_cast<int>(to_hresult());lastArtworkPath={};lastArtworkRevision={};
                // Even if the shell rejects clearing its thumbnail, still try
                // to commit the metadata and preserve the failure for callers.
                try{updater.Thumbnail(nullptr);}catch(...){}
            }
        } else if(artwork.empty()){
            lastArtworkPath={};lastArtworkRevision={};
            try{updater.Thumbnail(nullptr);}catch(...){artworkStatus=static_cast<int>(to_hresult());}
        }
        updater.Update();
        auto state = data.GetNamedString(L"state", L"stopped");
        auto speed = data.GetNamedNumber(L"speed", 1);
        controls.PlaybackRate(std::isfinite(speed) ? std::clamp(speed, -16.0, 16.0) : 1.0);
        controls.PlaybackStatus(state == L"playing" ? MediaPlaybackStatus::Playing : state == L"paused" ? MediaPlaybackStatus::Paused : MediaPlaybackStatus::Stopped);
        auto rawDuration = data.GetNamedNumber(L"durationMs", 0);
        auto rawPosition = data.GetNamedNumber(L"positionMs", 0);
        auto duration = std::isfinite(rawDuration) ? std::clamp(rawDuration, 0.0, 9e12) : 0.0;
        auto position = std::isfinite(rawPosition) ? std::clamp(rawPosition, 0.0, duration > 0 ? duration : 9e12) : 0.0;
        SystemMediaTransportControlsTimelineProperties timeline;
        timeline.StartTime(winrt::Windows::Foundation::TimeSpan{0});
        timeline.MinSeekTime(winrt::Windows::Foundation::TimeSpan{0});
        timeline.EndTime(winrt::Windows::Foundation::TimeSpan{static_cast<int64_t>(duration * 10000)});
        timeline.MaxSeekTime(timeline.EndTime());
        // No artificial seek range for live/unknown-duration sessions.
        timeline.Position(winrt::Windows::Foundation::TimeSpan{static_cast<int64_t>((duration > 0 ? position : 0.0) * 10000)});
        controls.UpdateTimelineProperties(timeline);
        return artworkStatus;
    } catch (...) { return static_cast<int>(to_hresult()); }
}
