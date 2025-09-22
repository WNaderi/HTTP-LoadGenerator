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
#include "ThreadPool.hpp"
#include "LoadGeneratorClient.hpp"

// Reserve memory for static global variables.
std::chrono::time_point<std::chrono::high_resolution_clock> LoadGeneratorClient::request_start_time;
std::chrono::duration<double, std::milli> LoadGeneratorClient::first_byte_latency;
std::mutex LoadGeneratorClient::latency_aggregation_lock;
std::mutex LoadGeneratorClient::first_byte_speed_aggregation_lock;
std::mutex LoadGeneratorClient::min_latency_lock;
std::mutex LoadGeneratorClient::max_latency_lock;

double LoadGeneratorClient::latency_aggregation;
double LoadGeneratorClient::first_byte_speed_aggregation;
double LoadGeneratorClient::min_latency = std::numeric_limits<double>::max();
double LoadGeneratorClient::max_latency= std::numeric_limits<double>::min();

// Global variables for main function arguments
char* user_url;
int user_num_req = 1;
LoadGeneratorClient::thread_type type = LoadGeneratorClient::per_request;
int pool_size;


size_t LoadGeneratorClient::timing_callback(char *buf, size_t size, size_t nmemb, void *userdata) {

    // Dont need anything for now
    // Simply need this so libcurl doesnt data print to screen

    // TO DO: Bytes Per Second (total response payload throughput)
    return size * nmemb;
}

void LoadGeneratorClient::Create_Send_Request(char* user_url) {

    CURL* handle = curl_easy_init();

    if (handle) {

        curl_easy_setopt(handle, CURLOPT_URL, user_url);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, timing_callback);

        request_start_time = std::chrono::high_resolution_clock::now();

        CURLcode curl_response = curl_easy_perform(handle);

        auto request_end_time = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> request_latency_ms = request_end_time - request_start_time;

        if (curl_response != CURLE_OK) {

            std::string curl_error = curl_easy_strerror(curl_response);
            throw std::runtime_error("curl_easy_perform() failed: " + curl_error);

        }


        double ttfb = 0, total_latency; // Time to First Byte, total transfer latency
        curl_easy_getinfo(handle, CURLINFO_STARTTRANSFER_TIME, &ttfb); // Get ttfb 
        curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME, &total_latency); // Get total response latency

        
        std::lock_guard<std::mutex> latency_lock(latency_aggregation_lock);
            latency_aggregation += total_latency;
        std::lock_guard<std::mutex> first_byte_lock(first_byte_speed_aggregation_lock);
            first_byte_speed_aggregation += ttfb;
        std::lock_guard<std::mutex> min_lock(min_latency_lock);
            min_latency = std::min(min_latency, total_latency);
        std::lock_guard<std::mutex> max_lock(max_latency_lock);
            max_latency = std::max(max_latency, total_latency);
            
            
    }

}

void LoadGeneratorClient::Run_Thread_Per_Request() {

    std::vector<std::thread> threads;
    for (int i = 0; i < num_of_reqs; i++) {

        threads.emplace_back(Create_Send_Request, url);

    }

    for (auto& t : threads)
        if (t.joinable()) t.join();

    Print_results(num_of_reqs); 
    Save_results(num_of_reqs);   

}

void LoadGeneratorClient::Run_Thread_Pool() {

    auto task = [=]() {

        LoadGeneratorClient::Create_Send_Request(user_url);

    };
    tpool = new ThreadPool(task, user_num_req, pool_size);
    tpool->Terminate_Thread_Pool();
    delete tpool;

}

void LoadGeneratorClient::Run() {

    switch (type) {

        case LoadGeneratorClient::thread_pool:
            Run_Thread_Pool();

        case LoadGeneratorClient::per_request:
            Run_Thread_Per_Request();


    }
    
}

LoadGeneratorClient::LoadGeneratorClient(char* user_url, int req_num_threads, thread_type type)
    : url(user_url), num_of_reqs(req_num_threads), type(type) {}


void LoadGeneratorClient::Print_results(int num_threads) {
    
    //Average Latency
    std::cout << "Average-Response-Latency " << latency_aggregation / num_threads;
    //First Byte Latency
    std::cout << " Average-First-Byte-Latency "  << first_byte_speed_aggregation / num_threads;
    //Min/Max Latency
    std::cout << " min/max-latency " << min_latency << "/" << max_latency;
    //Requests Per Second

    //Bytes Per Second

    //Success Rate/Error Rate

    //Error code breakdown

    std::cout << std::endl;

}

void LoadGeneratorClient::Save_results(int num_threads) {
    std::ofstream file("output.csv", std::ios::app);

    if (!file.is_open()) {
        std::cerr << "Error opening output.csv\n";
        return;
    }

    // Check if the file is empty to write the header only once
    static bool header_written = false;
    if (!header_written) {
        file << "Average-Response-Latency,Average-First-Byte-Latency,min/max-latency\n";
        header_written = true;
    }

    file << latency_aggregation / num_threads << ",";
    file << first_byte_speed_aggregation / num_threads << ",";
    file << min_latency << "/" << max_latency << "\n";
}

void Parse_Args(int argc, char* argv[]) {

    /*  
        Ex usage. ./LoadGeneratorClient -p (for pool) <url> <# of threads>
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

