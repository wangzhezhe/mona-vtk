#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <string>

struct Settings {
    size_t L;
    int steps;
    double F;
    double k;
    double dt;
    double Du;
    double Dv;
    double noise;
    std::string ssgfile;
    std::string loglevel;
    std::string protocol;
    std::string pipelinename;

    Settings();
    static Settings from_json(const std::string &fname);
};

#endif
