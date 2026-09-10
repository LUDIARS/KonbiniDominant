#include "konbini/app/playtest_options.h"
#include <fstream>
#include <stdexcept>
#include <string>
namespace konbini::app {
PlaytestOptions readPlaytestOptions(const std::filesystem::path& file) {
    if(!std::filesystem::exists(file)) return {};
    std::ifstream input(file);
    unsigned speed=0;std::string mode,extra;
    if(!(input>>speed>>mode) || (input>>extra) || speed<1 || speed>100 ||
       (mode!="manual" && mode!="autoplay"))
        throw std::runtime_error("playtest.cfg requires: <1..100> <manual|autoplay>");
    return {static_cast<double>(speed),mode=="autoplay",file.parent_path()/"playtest-report.jsonl"};
}
}
