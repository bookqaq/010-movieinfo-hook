#pragma once

#include <vector>
#include <string>

struct offsets_t
{
    std::string dll_version {};
    std::string dll_type {};

    std::size_t dll_code_size {};
    std::size_t dll_entrypoint {};
    std::size_t dll_image_size {};

    std::uintptr_t hook_iidxmusic_movieinfo_request {};
    std::uintptr_t hook_movie_upload_url_before {};
    std::uintptr_t hook_movie_upload_url_after {};
};
