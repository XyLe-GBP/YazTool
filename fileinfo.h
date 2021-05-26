#pragma once
#include <string>

int _path(std::string fpath) {
    int path_i = fpath.find_last_of("\\") + 1;    //    �t�@�C���p�X����A�Ō�̃f�B���N�g�����܂ł��������擾
    return path_i;
}
int _ext(std::string fpath) {
    int ext_i = fpath.find_last_of(".");        //    �t�@�C���p�X����A�Ō��.�܂ł̕������擾
    return ext_i;
}

std::string get_ext(std::string path) {
    std::string extname = path.substr(_ext(path));    //    �g���q�擾
    return extname;
}
std::string get_filename(std::string path) {
    std::string filename = path.substr(_path(path), _ext(path) - _path(path));    //    �t�@�C�����擾
    return filename;
}