#ifndef LOAD_GENERATOR_CLIENT_HPP
#define LOAD_GENERATOR_CLIENT_HPP

#include <curl/curl.h>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include "ThreadPool.hpp"


class LoadGeneratorClient {

public:
    enum thread_type : int;

private:

    static std::mutex metrics_lock;

    // Static metrics
    static double latency_aggregation;
    static double first_byte_speed_aggregation;
    static double min_latency;
    static double max_latency;
    static long long total_bytes;
    static int success_count;
    static int error_count;
    static std::map<long, int> status_code_breakdown;
    static std::map<std::string, int> curl_error_breakdown;
    static std::chrono::duration<double> total_run_time;

    // Libcurl callback
    static size_t timing_callback(char* buf, size_t size, size_t nmemb, void* userdata);

    // Sends a request to the provided URL
    static void Create_Send_Request(char* user_url);

    // User-specified configuration
    char* url;
    int num_of_reqs;
    thread_type type;
    bool save_to_csv;
    std::string csv_output_path;

    // Thread Pool Manager
    ThreadPool* tpool;

    // Executes a thread per request and aggregates metrics
    void Run_Thread_Per_Request();

    // Executes threads with thread pool
    void Run_Thread_Pool();

    // Prints metrics to stdout
    void Print_results(int num_requests);

    // Saves metrics to CSV
    void Save_results(int num_requests);

public:

    enum thread_type : int {

        per_request,
        thread_pool

    };

    LoadGeneratorClient(char* user_url, int req_num_threads, thread_type type, bool save_to_csv, const std::string& csv_output_path);

    void Run();
    
};

#endif // LOAD_GENERATOR_CLIENT_HPP
