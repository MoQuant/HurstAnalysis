#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <thread>
#include <curl/curl.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace boost::property_tree;

std::string historicalTable(std::string ticker, std::string interval, std::string t0, std::string t1){
    std::string key = ""; // Enter FMP Key here
    std::string url = "https://financialmodelingprep.com/api/v3/historical-chart/" + interval + "/" + ticker + "?from=" + t0 + "&to=" + t1 + "&apikey=" + key;
    return url;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t totalSize = size * nmemb;
    data->append((char*)contents, totalSize);
    return totalSize;
}

std::string Request(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init(); 
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response); 

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return response;
}

double MU(std::vector<double> x){
    double total = 0;
    for(auto & i : x){
        total += i;
    }
    total /= (double) x.size();
    return total;
}

double SD(std::vector<double> x){
    double total = 0;
    double mean = MU(x);
    for(auto & i : x){
        total += pow(i - mean, 2);
    }
    total /= ((double) x.size() - 1);
    return pow(total, 0.5);
}

double HurstExponent(std::vector<double> price){
    std::vector<double> ror, diff;
    for(int i = 1; i < price.size(); ++i){
        ror.push_back(price[i]/price[i-1] - 1.0);
    }
    double rormu = MU(ror);
    double csum = 0;
    for(int i = 0; i < ror.size(); ++i){
        csum += ror[i];
        diff.push_back(csum - rormu);
    }
    double sigma = SD(ror);
    std::sort(diff.begin(), diff.end());
    double Y0 = diff[0], Y1 = diff[diff.size() - 1];

    return log((Y1 - Y0)/sigma)/log(ror.size());
}

void Typhoon(std::string ticker, std::string interval, std::string t0, std::string t1, std::map<std::string, std::map<std::string, double>> & data){
    std::string response = Request(historicalTable(ticker, interval, t0, t1));
    std::stringstream ss(response);
    ptree df;
    read_json(ss, df);
    std::vector<double> close;
    for(ptree::const_iterator it = df.begin(); it != df.end(); ++it){
        for(ptree::const_iterator jt = it->second.begin(); jt != it->second.end(); ++jt){
            if(jt->first == "close"){
                close.push_back(atof(jt->second.get_value<std::string>().c_str()));
            }
        }
    }
    std::reverse(close.begin(), close.end());
    data[ticker][interval] = HurstExponent(close);
}

int main()
{
    std::vector<std::string> intervals = {"1min", "5min", "15min", "30min", "1hour", "4hour"};
    std::vector<std::string> tickers = {"AAPL","MSFT","GS","AMZN","NVDA","WMT","HD","BLK","JPM"};

    std::string t0 = "2024-01-01";
    std::string t1 = "2024-10-29";

    std::map<std::string, std::map<std::string, double>> data;

    std::vector<std::thread> process;
    for(auto & ticker : tickers){
        for(auto & interval : intervals){
            process.emplace_back(Typhoon, ticker, interval, t0, t1, std::ref(data));
        }
    }


    for(int t = 0; t < process.size(); ++t){
        process[t].join();
    }

    for(int k = 0; k < tickers.size(); ++k){
        for(int q = 0; q < intervals.size(); ++q){
            std::cout << tickers[k] << " | " << intervals[q] << " | ";
            if(data[tickers[k]][intervals[q]] > 0.5){
                std::cout << "TR" << std::endl;
            } else if(data[tickers[k]][intervals[q]] < 0.5){
                std::cout << "MR" << std::endl;
            } else {
                std::cout << "EF" << std::endl;
            }
        }
        std::cout << std::endl;
    }


    
    return 0;
}
