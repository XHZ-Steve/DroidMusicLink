#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Data.Json.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <vector>

using namespace winrt;
using namespace Windows::Storage::Streams;
static constexpr wchar_t testId[]=L"DroidMediaLink.SMTC.ArtworkTest";
static std::vector<uint8_t> bitmap(uint8_t color){
    BITMAPFILEHEADER file{};BITMAPINFOHEADER image{};
    image.biSize=sizeof(image);image.biWidth=2;image.biHeight=2;image.biPlanes=1;image.biBitCount=24;image.biSizeImage=16;
    file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(image);file.bfSize=file.bfOffBits+image.biSizeImage;
    std::vector<uint8_t> bytes(file.bfSize,color);memcpy(bytes.data(),&file,sizeof(file));memcpy(bytes.data()+sizeof(file),&image,sizeof(image));return bytes;
}
static bool verifySystemThumbnail(std::vector<uint8_t> expected){
    auto result=std::async(std::launch::async,[expected=std::move(expected)]{
        init_apartment(apartment_type::multi_threaded);
        auto manager=Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        const auto deadline=GetTickCount64()+5000;bool recognized=false,thumbnailPresent=false;
        do{
            for(auto const& session:manager.GetSessions()){
                // Never inspect or control another player's session.
                const auto identity=std::wstring(session.SourceAppUserModelId());
                if(identity!=testId&&identity.find(L"SmtcArtworkTests")==std::wstring::npos)continue;
                auto properties=session.TryGetMediaPropertiesAsync().get();
                if(properties.Title()!=L"TEST SESSION")continue;
                recognized=true;
                auto thumbnail=properties.Thumbnail();
                thumbnailPresent=bool(thumbnail);
                if(!thumbnail){if(expected.empty())return true;continue;}
                if(expected.empty())continue;
                auto stream=thumbnail.OpenReadAsync().get();
                if(stream.Size()!=expected.size())continue;
                DataReader reader(stream.GetInputStreamAt(0));
                if(reader.LoadAsync(static_cast<uint32_t>(expected.size())).get()!=expected.size())continue;
                std::vector<uint8_t> actual(expected.size());reader.ReadBytes(actual);
                if(actual==expected)return true;
            }
            Sleep(50);
        }while(GetTickCount64()<deadline);
        std::cout<<"Probe timeout: own session recognized="<<recognized<<" thumbnail present="<<thumbnailPresent<<'\n';return false;
    });
    while(result.wait_for(std::chrono::milliseconds(10))!=std::future_status::ready){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}}
    return result.get();
}
int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 64;
    try{
        init_apartment(apartment_type::single_threaded);
        check_hresult(SetCurrentProcessExplicitAppUserModelID(testId));
        auto window=CreateWindowExW(0,L"STATIC",L"DroidMediaLink SMTC TEST",WS_OVERLAPPEDWINDOW,0,0,320,160,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!window)return 2;
        ShowWindow(window,SW_SHOWNOACTIVATE);
        auto library=LoadLibraryW(argv[1]);if(!library)return 3;
        using Init=int(__cdecl*)(HWND,void(__cdecl*)(const char*,double));
        using Update=int(__cdecl*)(const char*);using Shutdown=void(__cdecl*)();
        auto initialize=reinterpret_cast<Init>(GetProcAddress(library,"dml_smtc_init"));
        auto update=reinterpret_cast<Update>(GetProcAddress(library,"dml_smtc_update"));
        auto shutdown=reinterpret_cast<Shutdown>(GetProcAddress(library,"dml_smtc_shutdown"));
        if(!initialize||!update||!shutdown||initialize(window,nullptr)!=1)return 4;
        const auto directory=std::filesystem::temp_directory_path()/(L"DML-SMTC-封面-"+std::to_wstring(GetCurrentProcessId()));
        std::filesystem::create_directory(directory);const auto path=directory/L"封面 测试.bmp";
        const auto first=bitmap(40),second=bitmap(180);
        auto write=[&](const auto& bytes){std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!file)throw_hresult(E_FAIL);};
        auto publish=[&](hstring artwork,hstring revision){
            Windows::Data::Json::JsonObject data;
            data.SetNamedValue(L"online",Windows::Data::Json::JsonValue::CreateBooleanValue(true));
            auto commands=Windows::Data::Json::JsonArray::Parse(L"[\"play\",\"pause\",\"next\",\"previous\"]");
            data.SetNamedValue(L"commands",commands);
            data.SetNamedValue(L"durationMs",Windows::Data::Json::JsonValue::CreateNumberValue(180000));
            for(auto const& pair:std::vector<std::pair<hstring,hstring>>{{L"title",L"TEST SESSION"},{L"artist",L"TEST ARTWORK"},{L"state",L"playing"},{L"artwork",artwork},{L"artworkRevision",revision}})data.SetNamedValue(pair.first,Windows::Data::Json::JsonValue::CreateStringValue(pair.second));
            return update(to_string(data.Stringify()).c_str());
        };
        write(first);auto forward=path.generic_wstring();
        auto pathProbe=std::async(std::launch::async,[&]{
            init_apartment(apartment_type::multi_threaded);
            for(auto const& entry:std::vector<std::pair<const char*,std::wstring>>{{"forward",forward},{"native",path.wstring()}}){
                int status=0;try{Windows::Storage::StorageFile::GetFileFromPathAsync(hstring(entry.second)).get();}catch(...){status=static_cast<int>(to_hresult());}
                std::cout<<"StorageFile path probe (MTA) "<<entry.first<<" HRESULT="<<std::hex<<status<<std::dec<<'\n';
            }
        });pathProbe.get();
        int status=publish(hstring(forward),L"A");bool passed=status==0&&verifySystemThumbnail(first);
        std::cout<<"STA / Unicode / forward-slash path: "<<passed<<" status="<<std::hex<<status<<std::dec<<'\n';
        if(passed){write(second);status=publish(hstring(path.wstring()),L"B");passed=status==0&&verifySystemThumbnail(second);std::cout<<"Changed bytes at same path: "<<passed<<'\n';}
        if(passed){write(first);status=publish(hstring(path.wstring()),L"B");passed=status==0&&verifySystemThumbnail(second);std::cout<<"Published snapshot survives source overwrite: "<<passed<<'\n';}
        if(passed){status=publish(hstring(path.wstring()),L"");passed=status==0&&verifySystemThumbnail(first);std::cout<<"Legacy payload without revision refreshes thumbnail: "<<passed<<'\n';}
        if(passed){std::filesystem::remove(path);status=publish(hstring(path.wstring()),L"RETRY");passed=status!=0&&verifySystemThumbnail({});std::cout<<"Missing file reports failure without stale thumbnail: "<<passed<<'\n';}
        if(passed){write(first);status=publish(hstring(path.wstring()),L"RETRY");passed=status==0&&verifySystemThumbnail(first);std::cout<<"Same revision retries after file becomes available: "<<passed<<'\n';}
        if(passed){status=publish(L"",L"");passed=status==0&&verifySystemThumbnail({});std::cout<<"No-cover update clears system thumbnail: "<<passed<<'\n';}
        shutdown();DestroyWindow(window);FreeLibrary(library);
        std::filesystem::remove(path);std::filesystem::remove(directory);
        return passed?0:5;
    }catch(hresult_error const& error){std::cerr<<"Native artwork test HRESULT="<<std::hex<<error.code().value<<'\n';return 6;}
}
