#ifndef QUERY_H
#define QUERY_H

#include <json/json.h>
#include <curl/curl.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include <sstream>
#include <vector>

class Query{
    public:
    std::vector<double> makeQuery();
};

#endif