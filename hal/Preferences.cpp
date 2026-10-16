#include "Preferences.h"

std::map<std::string, std::map<std::string, std::string>> Preferences::_storage;
std::mutex Preferences::_mtx;
