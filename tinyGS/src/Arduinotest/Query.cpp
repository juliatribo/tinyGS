#include "Query.h"
#include <vector>

using namespace std;

vector<double> Query::makeQuery(){
    vector<double> output;

    curlpp::Cleanup myCleanup;

    //The apikey to acces the API
    std::string apiKey = "";//API key value
    //The id of the satellite
    std::string id = "25544";
    //observer postiion
    std::string pos = "/41.38906959985834/2.1125049211069404/20";
    //Number of future positions to return. Each second is a position. Limit 300 seconds
    std::string seconds = "/1";
    
    std::ostringstream os;
    os << curlpp::options::Url("https://api.n2yo.com/rest/v1/satellite/positions/"+id+pos+seconds+"/&"+apiKey);

    //we get the response as a string
    std::string response = os.str();

    //from the response (string) we build a data (json)
    Json::Reader reader;
    Json::Value data;
    reader.parse(response, data);

    //From the json data we access to the parameters that are in position
    
    output = {data["positions"][0]["azimuth"].asDouble() ,data["positions"][0]["elevation"].asDouble()}; //{azimuth,elevation}

    return output;
}
