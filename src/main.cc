#include <versions.h>
#include <Windows.h>
#include <winhttp.h>
#include <safetyhook.hpp>
#include "avs2-log.h"
#include "parse_http_link.h"

/* constants */
auto constexpr game_dll_name = {"bm2dx.dll", "bm2dx_omni.dll"};

/* app context */
auto static init = std::once_flag {};

/* game context */
auto static bm2dx = PBYTE {};
auto static offsets = offsets_t {};

/* game hooks */
auto static hook_movie_upload_url_before = SafetyHookMid {};
auto static hook_movie_upload_url_after = SafetyHookMid {};
auto static hook_iidxmusic_movieinfo_request = SafetyHookInline {};

/* game data storage */
uintptr_t p_movie_upload_url = NULL;
std::string movie_upload_addr;
uint16_t movie_upload_port;

/* http request related */
int request_timeout = 3000;     // 3000 millisecond
HINTERNET http_session;
LPCWSTR APIFeatureXrpcIIDXMovieInfo = L"/feature/xrpcIIDXMovieInfo";
LPCWSTR headers = L"Content-Type: application/json\r\n";

/* entrypoint */
auto DllMain(void*, unsigned long reason, void*)
{
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;

    // initialize http session
    http_session = WinHttpOpen(L"010-movieinfo-hook/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (http_session == nullptr) {
        avs2::log::warning("HTTP Session failed to initialize (code {}), hook will not be activated...", GetLastError());
        return FALSE;
    }

    /* find game library */
    for (auto it = game_dll_name.begin(); !bm2dx && it != game_dll_name.end(); ++it)
        bm2dx = reinterpret_cast<PBYTE>(GetModuleHandleA(*it));
    if (!bm2dx)
        return FALSE;

    /* determine game version */
    auto const dos = reinterpret_cast<PIMAGE_DOS_HEADER>(bm2dx);
    auto const nt = reinterpret_cast<PIMAGE_NT_HEADERS>(bm2dx + dos->e_lfanew);
    auto const it = std::ranges::find_if(versions, [&](auto const& info)
    {
        return info.dll_code_size == nt->OptionalHeader.SizeOfCode
            && info.dll_entrypoint == nt->OptionalHeader.AddressOfEntryPoint
            && info.dll_image_size == nt->OptionalHeader.SizeOfImage;
    });

    if (it == versions.end()) {
        avs2::log::warning("can't match any bm2dx.dll supported, hook will not be activated...");
        return FALSE;
    }

    offsets = *it;
    avs2::log::warning("010-movieinfo-hook match {}-{}", offsets.dll_type, offsets.dll_version);

    /* install hooks */
    // get movie upload url to determine api host:port
    // there are other places where we can get only ip and port, but this
    // is the most obvious one. so we'll hook here
    // NOTE: we have to access r9, so safetyhook::Context64
    hook_movie_upload_url_before = safetyhook::create_mid(bm2dx + offsets.hook_movie_upload_url_before, +[] (safetyhook::Context64& ctx)
    {
        // get r9
        p_movie_upload_url = ctx.r9;
        avs2::log::info("get r9: {}", p_movie_upload_url);
    });

    // get real url and parse ip:port. it might be better to combine 
    // two mid hooks into a inline hook.
    hook_movie_upload_url_after = safetyhook::create_mid(bm2dx + offsets.hook_movie_upload_url_after, +[] (safetyhook::Context64& ctx)
    {
        size_t pos_end_of_endpoint = 0;
        size_t pos_double_slice = 0;
        char* temp_movie_upload_url = nullptr;
        std::string got_url;
        int res;
        UrlComponents* compoments = new UrlComponents;

        if (compoments == nullptr) {
            goto END;
        }

        if (p_movie_upload_url == NULL) {
            goto END;
        }

        temp_movie_upload_url = reinterpret_cast<char*>(p_movie_upload_url);
        // convert r9 stored to string
        got_url.assign(temp_movie_upload_url, strlen(temp_movie_upload_url));
        avs2::log::info("got movie upload endpoint: {}", got_url);

        // try to parse addr
        res = parseUrlManual(compoments, got_url);
        if (res > 0) {
            avs2::log::warning("cant parse url successfully: {}", res);
            goto END;
        }

        // set addr and port to outside variable
        movie_upload_addr.assign(compoments->host);
        movie_upload_port = compoments->port;

        END:    // this is just a tag for easy returning
        avs2::log::info("current ip and port: {}:{}", movie_upload_addr, movie_upload_port);
    });

    // hook IIDX00music.movieinfo request marshal function to intercept and get data.
    // after that send it to the server
    hook_iidxmusic_movieinfo_request = safetyhook::create_inline(bm2dx + offsets.hook_iidxmusic_movieinfo_request, +[] (void* a1, void* a2)
    {
        // if we don't get url yet, do nothing
        if (!movie_upload_addr.empty()) {
            HINTERNET h_connect;                                    // connection to server
            HINTERNET h_request;                                    // one http request context
            DWORD res_status_code = 0;                              // response status code
            DWORD dw_status_code_size = sizeof(res_status_code);    // size of status code
            std::string response_string;                            // response body data
            DWORD dw_size = 0;                                      // response body size

            // get struct from a1+112
            // v3 = *(unsigned int **)(a1 + 112);
            uintptr_t addr_a1 = reinterpret_cast<uintptr_t>(a1);
            addr_a1 += 112;
            void** p_to_movieinfo_request = reinterpret_cast<void**>(addr_a1);
            // deference and then cast to byte*
            uintptr_t p_movieinfo_request = reinterpret_cast<uintptr_t>(*p_to_movieinfo_request);
            char* p_movieinfo_request_iidxid = reinterpret_cast<char*>(p_movieinfo_request);
            char* p_movieinfo_request_session_id = reinterpret_cast<char*>(p_movieinfo_request+0x80);

            // get iidxid from p_movieinfo_request+0 (length 8 byte)
            char c_iidxid[9];
            std::memcpy(c_iidxid, p_movieinfo_request_iidxid, 8);
            c_iidxid[8] = 0;    // force end string
            avs2::log::misc("iidxid c string: {}", c_iidxid);
            auto iidxid = std::string(c_iidxid);
            avs2::log::misc("movieinfo request get iidxid: {}", iidxid);
            
            // get session_id from p_movieinfo_request+0x80 (length might be 32 byte)
            char c_session_id[33];
            std::memcpy(&c_session_id, p_movieinfo_request_session_id, 32);
            c_session_id[32] = 0;
            avs2::log::misc("session_id c string: {}", c_session_id);
            auto session_id = std::string(c_session_id);
            avs2::log::misc("movieinfo request get session_id: {}", session_id);

            // build body
            std::string template_request_body = "{\"iidxid\":\"%s\",\"session_id\":\"%s\"}";

            // https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
            // get size
            int body_size = std::snprintf(nullptr, 0, template_request_body.c_str(), iidxid, session_id) + 1;
            auto body_size_t = static_cast<size_t>(body_size);
            avs2::log::misc("calculate body size: {}", body_size_t);
            // create buffer
            std::unique_ptr<char[]> body_buf(new char[body_size_t]);
            // format string
            std::snprintf(body_buf.get(),body_size_t ,template_request_body.c_str(), iidxid, session_id);
            auto requestbody = std::string(body_buf.get(), body_buf.get() + body_size_t - 1);
            avs2::log::info("build request body: {}", requestbody);

            // convert movie_upload_addr to LPCWSTR (single-byte character only)
            // who will put unicode char in domain name?
            std::wstring stemp = std::wstring(movie_upload_addr.begin(), movie_upload_addr.end());
            LPCWSTR w_movie_upload_addr = stemp.c_str();

            // send request to server using http (thanks gemini again)
            h_connect = WinHttpConnect(http_session, w_movie_upload_addr, movie_upload_port, 0);
            if (h_connect == nullptr) {
                avs2::log::warning("WinHttpConnect failed (code {})", GetLastError());
                goto END;
            }
            h_request = WinHttpOpenRequest(h_connect, L"POST",APIFeatureXrpcIIDXMovieInfo, NULL,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_REFRESH);
            if (h_request == nullptr) {
                avs2::log::warning("WinHttpOpenRequest failed (code {})", GetLastError());
                WinHttpCloseHandle(h_connect);
                goto END;
            }
            if (!WinHttpSetTimeouts(h_request, request_timeout, request_timeout, request_timeout, request_timeout)) {
                avs2::log::warning("WinHttpSetTimeouts failed (code {})", GetLastError());
                WinHttpCloseHandle(h_request);
                WinHttpCloseHandle(h_connect);
                goto END;
            }
            if (!WinHttpAddRequestHeaders(h_request, headers, (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD)) {
                avs2::log::warning("WinHttpAddRequestHeaders failed (code {})", GetLastError());
                WinHttpCloseHandle(h_request);
                WinHttpCloseHandle(h_connect);
                goto END;
            }
            if (!WinHttpSendRequest(h_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)requestbody.c_str(),
                (DWORD)requestbody.length(), (DWORD)requestbody.length(), 0)) {
                avs2::log::warning("WinHttpSendRequest failed (code {})", GetLastError());
                WinHttpCloseHandle(h_request);
                WinHttpCloseHandle(h_connect);
                goto END;
            }
            if (!WinHttpReceiveResponse(h_request, NULL)) {
                avs2::log::warning("WinHttpReceiveResponse failed (code {})", GetLastError());
                WinHttpCloseHandle(h_request);
                WinHttpCloseHandle(h_connect);
                goto END;
            }
            if (!WinHttpQueryHeaders(h_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &res_status_code, &dw_status_code_size, WINHTTP_NO_HEADER_INDEX)) {
                avs2::log::warning("WinHttpQueryHeaders get status code failed (code {})", GetLastError());
                WinHttpCloseHandle(h_request);
                WinHttpCloseHandle(h_connect);
                goto END;
            }
            do {
                dw_size = 0;
                if (!WinHttpQueryDataAvailable(h_request, &dw_size)) {
                    avs2::log::warning("WinHttpQueryDataAvailable failed (code {})", GetLastError());
                    break;
                }
                if (dw_size > 0) {
                    std::vector<char> buffer(dw_size + 1, 0);
                    DWORD dwRead = 0;
                    if (WinHttpReadData(h_request, &buffer[0], dw_size, &dwRead)) {
                        response_string.append(buffer.begin(), buffer.begin() + dwRead);
                    }
                    else {
                        avs2::log::warning("WinHttpReadData failed (code {})", GetLastError());
                    }
                }
            } while (dw_size > 0);

            avs2::log::info("Receive response from server. code:{}  body: {}", res_status_code, response_string);
            WinHttpCloseHandle(h_request);
            WinHttpCloseHandle(h_connect);
        }

        END: /* call original function */
        return hook_iidxmusic_movieinfo_request.call<void*, void*>(a1, a2);
    });

    return TRUE;
}