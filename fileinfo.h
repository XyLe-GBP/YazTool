#pragma once
#include <string>

int _path(std::string fpath) {
    int path_i = fpath.find_last_of("\\") + 1;    //    ファイルパスから、最後のディレクトリ名までも文字数取得
    return path_i;
}
int _ext(std::string fpath) {
    int ext_i = fpath.find_last_of(".");        //    ファイルパスから、最後の.までの文字数取得
    return ext_i;
}

std::string get_ext(std::string path) {
    std::string extname = path.substr(_ext(path));    //    拡張子取得
    return extname;
}
std::string get_filename(std::string path) {
    std::string filename = path.substr(_path(path), _ext(path) - _path(path));    //    ファイル名取得
    return filename;
}