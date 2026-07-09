#include <curl/curl.h>
#include <stdio.h>
#include <stdexcept>
#include <chrono>
#include <iostream>
#include <cstring>
#include <vector>
#include <thread>
#include <mutex>
#include <fstream>
#include <cctype>
#include <sstream>
#include "ThreadPool.hpp"
#include "LoadGeneratorClient.hpp"

std::mutex LoadGeneratorClient::metrics_lock;

double LoadGeneratorClient::latency_aggregation;
double LoadGeneratorClient::first_byte_speed_aggregation;
double LoadGeneratorClient::min_latency = std::numeric_limits<double>::max();
double LoadGeneratorClient::max_latency= std::numeric_limits<double>::min();
long long LoadGeneratorClient::total_bytes = 0;
int LoadGeneratorClient::success_count = 0;
int LoadGeneratorClient::error_count = 0;
std::map<long, int> LoadGeneratorClient::status_code_breakdown;
std::map<std::string, int> LoadGeneratorClient::curl_error_breakdown;
std::chrono::duration<double> LoadGeneratorClient::total_run_time;

// Global variables for main function arguments
char* user_url;
int user_num_req = 1;
LoadGeneratorClient::thread_type type = LoadGeneratorClient::per_request;
int pool_size;

template <typename K>
std::string Format_Breakdown(const std::map<K, int>& breakdown) {
    std::ostringstream output;
    bool first = true;

    for (const auto& entry : breakdown) {
        if (!first)
            output << ";";
        output << entry.first << ":" << entry.second;
        first = false;
    }

    return output.str();
}

std::string Csv_Header() {
    return "Average-Response-Latency,Average-First-Byte-Latency,min-latency,max-latency,"
           "requests-per-second,bytes-per-second,success-rate,error-rate,"
           "success-count,error-count,status-code-breakdown,curl-error-breakdown";
}

size_t LoadGeneratorClient::timing_callback(char *buf, size_t size, size_t nmemb, void *userdata) {

    size_t bytes_received = size * nmemb;
    size_t* response_bytes = static_cast<size_t*>(userdata);

    if (response_bytes)
        *response_bytes += bytes_received;

    return bytes_received;
}

void LoadGeneratorClient::Create_Send_Request(char* user_url) {

    CURL* handle = curl_easy_init();

    if (handle) {
        size_t response_bytes = 0;

        curl_easy_setopt(handle, CURLOPT_URL, user_url);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, timing_callback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_bytes);

        CURLcode curl_response = curl_easy_perform(handle);

        double ttfb = 0, total_latency = 0; // Time to First Byte, total transfer latency
        long response_code = 0;
        curl_easy_getinfo(handle, CURLINFO_STARTTRANSFER_TIME, &ttfb); // Get ttfb 
        curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME, &total_latency); // Get total response latency
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);

        {
            std::lock_guard<std::mutex> metrics_guard(metrics_lock);
            latency_aggregation += total_latency;
            first_byte_speed_aggregation += ttfb;
            min_latency = std::min(min_latency, total_latency);
            max_latency = std::max(max_latency, total_latency);
            total_bytes += response_bytes;

            if (response_code > 0)
                status_code_breakdown[response_code]++;

            if (curl_response == CURLE_OK && response_code >= 200 && response_code < 400) {
                success_count++;
            } else {
                error_count++;

                if (curl_response != CURLE_OK)
                    curl_error_breakdown[curl_easy_strerror(curl_response)]++;
            }
        }

        curl_easy_cleanup(handle);
    } else {
        std::lock_guard<std::mutex> metrics_guard(metrics_lock);
        error_count++;
        curl_error_breakdown["curl_easy_init failed"]++;
    }

}

void LoadGeneratorClient::Run_Thread_Per_Request() {

    std::vector<std::thread> threads;
    auto run_start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_of_reqs; i++) {

        threads.emplace_back(Create_Send_Request, url);

    }

    for (auto& t : threads)
        if (t.joinable()) t.join();

    auto run_end_time = std::chrono::high_resolution_clock::now();
    total_run_time = run_end_time - run_start_time;

    Print_results(num_of_reqs); 
    Save_results(num_of_reqs);   

}

void LoadGeneratorClient::Run_Thread_Pool() {
    auto run_start_time = std::chrono::high_resolution_clock::now();

    auto task = [=]() {

        LoadGeneratorClient::Create_Send_Request(url);

    };
    tpool = new ThreadPool(task, user_num_req, pool_size);
    tpool->Terminate_Thread_Pool();

    auto run_end_time = std::chrono::high_resolution_clock::now();
    total_run_time = run_end_time - run_start_time;

    Print_results(num_of_reqs); 
    Save_results(num_of_reqs);
    
    delete tpool;

}

void LoadGeneratorClient::Run() {

    switch (type) {

        case LoadGeneratorClient::thread_pool:
            Run_Thread_Pool();
            break;

        case LoadGeneratorClient::per_request:
            Run_Thread_Per_Request();


    }
    
}

LoadGeneratorClient::LoadGeneratorClient(char* user_url, int req_num_threads, thread_type type)
    : url(user_url), num_of_reqs(req_num_threads), type(type) {}


void LoadGeneratorClient::Print_results(int num_requests) {
    int completed_requests = success_count + error_count;
    double run_seconds = total_run_time.count();
    double requests_per_second = run_seconds > 0 ? completed_requests / run_seconds : 0;
    double bytes_per_second = run_seconds > 0 ? total_bytes / run_seconds : 0;
    double success_rate = completed_requests > 0 ? (success_count * 100.0) / completed_requests : 0;
    double error_rate = completed_requests > 0 ? (error_count * 100.0) / completed_requests : 0;
    
    //Average Latency
    std::cout << "Average-Response-Latency " << latency_aggregation / num_requests;
    //First Byte Latency
    std::cout << " Average-First-Byte-Latency "  << first_byte_speed_aggregation / num_requests;
    //Min/Max Latency
    std::cout << " min/max-latency " << min_latency << "/" << max_latency;
    //Requests Per Second
    std::cout << " requests-per-second " << requests_per_second;

    //Bytes Per Second
    std::cout << " bytes-per-second " << bytes_per_second;

    //Success Rate/Error Rate
    std::cout << " success-rate " << success_rate << "%";
    std::cout << " error-rate " << error_rate << "%";

    //Error code breakdown
    std::cout << " status-code-breakdown " << Format_Breakdown(status_code_breakdown);
    std::cout << " curl-error-breakdown " << Format_Breakdown(curl_error_breakdown);

    std::cout << std::endl;

}

void LoadGeneratorClient::Save_results(int num_requests) {
    bool write_header = true;
    std::ifstream existing_file("output.csv");
    std::string first_line;

    if (std::getline(existing_file, first_line))
        write_header = first_line != Csv_Header();

    std::ofstream file("output.csv", std::ios::app);

    if (!file.is_open()) {
        std::cerr << "Error opening output.csv\n";
        return;
    }

    int completed_requests = success_count + error_count;
    double run_seconds = total_run_time.count();
    double requests_per_second = run_seconds > 0 ? completed_requests / run_seconds : 0;
    double bytes_per_second = run_seconds > 0 ? total_bytes / run_seconds : 0;
    double success_rate = completed_requests > 0 ? (success_count * 100.0) / completed_requests : 0;
    double error_rate = completed_requests > 0 ? (error_count * 100.0) / completed_requests : 0;

    if (write_header)
        file << Csv_Header() << "\n";

    file << latency_aggregation / num_requests << ",";
    file << first_byte_speed_aggregation / num_requests << ",";
    file << min_latency << ",";
    file << max_latency << ",";
    file << requests_per_second << ",";
    file << bytes_per_second << ",";
    file << success_rate << ",";
    file << error_rate << ",";
    file << success_count << ",";
    file << error_count << ",";
    file << Format_Breakdown(status_code_breakdown) << ",";
    file << Format_Breakdown(curl_error_breakdown) << "\n";
}

void Parse_Args(int argc, char* argv[]) {

    /*  
        Usage:
            ./http-loadgen <url> <request-count>
            ./http-loadgen -p <pool-size> <url> <request-count>

        The final positional argument is the number of requests to send.
        In thread-pool mode, -p controls how many worker threads process those requests.
    */

    if (argc < 2) {

        throw std::invalid_argument("Minimum Usage: ./http-loadgen <url>");

    }

    int i = 1; // start at arg 1.
    for (; i < argc; i++) {

        if (argv[i][0] != '-') {

            // No more flags expected, begin parsing positional args.
            break;

        } else { // Parse flags
            switch (argv[i][1]) {


                case 'p': // Thread Pool flag.

                    type = LoadGeneratorClient::thread_pool;

                    // Extract thread pool size.
                    if (i+1 < argc && !isalpha(argv[i+1][0])) {

                        i++;
                        pool_size = atoi(argv[i]);
                    
                    } else {

                        throw std::invalid_argument("Must specify pool size.");


                    }

                    break;
                case 't': // Thread per request flag.

                    type = LoadGeneratorClient::per_request;
                    break;

            }

        }

    }

    if (i == argc) { // Whoops we are missing the positional args!
    
        throw std::invalid_argument("Must specify URL & request count.");

    }

    user_url = argv[i++];

    if (i != argc)
        user_num_req = atoi(argv[i]);

}

int main(int argc, char* argv[]) {

    
    Parse_Args(argc, argv);

    LoadGeneratorClient loadgen(user_url, user_num_req, type);

    loadgen.Run();

}
