#ifndef LOAD_GENERATOR_CLIENT_HPP
#define LOAD_GENERATOR_CLIENT_HPP

#include <curl/curl.h>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include "ThreadPool.hpp"


class LoadGeneratorClient {

public:
    enum thread_type : int;

private:

    // Static mutexes for thread safety
    static std::mutex latency_aggregation_lock;
    static std::mutex first_byte_speed_aggregation_lock;
    static std::mutex min_latency_lock;
    static std::mutex max_latency_lock;

    // Static metrics
    static double latency_aggregation;
    static double first_byte_speed_aggregation;
    static double min_latency;
    static double max_latency;

    // Static timing variables
    static std::chrono::time_point<std::chrono::high_resolution_clock> request_start_time;
    static std::chrono::duration<double, std::milli> first_byte_latency;

    // Libcurl callback
    static size_t timing_callback(char* buf, size_t size, size_t nmemb, void* userdata);

    // Sends a request to the provided URL
    static void Create_Send_Request(char* user_url);

    // User-specified configuration
    char* url;
    int num_of_reqs;
    thread_type type;

    // Thread Pool Manager
    ThreadPool* tpool;

    // Executes a thread per request and aggregates metrics
    void Run_Thread_Per_Request();

    // Executes threads with thread pool
    void Run_Thread_Pool();

    // Prints metrics to stdout
    void Print_results(int num_threads);

    // Saves metrics to CSV
    void Save_results(int num_threads);

public:

    enum thread_type : int {

        per_request,
        thread_pool

    };

    LoadGeneratorClient(char* user_url, int req_num_threads, thread_type type);

    void Run();
    
};

#endif // LOAD_GENERATOR_CLIENT_HPP
