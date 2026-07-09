# HTTP-LoadGenerator

HTTP-LoadGenerator is a small C++ command-line load testing tool. It sends many HTTP requests to a target URL using libcurl, runs those requests either with one thread per request or with a fixed-size thread pool, then reports latency, throughput, success/error rates, and response-code breakdowns.

The repository also includes a simple threaded Python HTTP server for local testing.

## Features

- Sends a configurable number of HTTP requests to a URL.
- Supports two execution modes:
  - thread per request
  - fixed-size thread pool
- Uses libcurl for HTTP request execution and timing information.
- Reports metrics to stdout.
- Appends metrics to `output.csv`.
- Includes a local threaded HTTP server on `localhost:8000` for smoke testing.

## Requirements

- C++ compiler with C++11 thread support
- libcurl development files
- `curl-config`
- Python 3, only needed for the included local test server

On Ubuntu/Debian-like systems, the libcurl dependency is usually:

```bash
sudo apt install g++ libcurl4-openssl-dev
```

## Build

```bash
bash compile.sh
```

This produces:

```text
./http-loadgen
```

## Usage

Thread per request mode:

```bash
./http-loadgen <url> <request-count>
```

Thread pool mode:

```bash
./http-loadgen -p <pool-size> <url> <request-count>
```

Arguments:

- `<url>`: target HTTP URL, such as `http://localhost:8000/`
- `<request-count>`: total number of requests to send
- `<pool-size>`: number of worker threads used in thread-pool mode

The final positional argument is the number of requests, not the number of threads. In thread-pool mode, `-p <pool-size>` controls the number of worker threads.

## Local Testing

Start the included Python server in one terminal:

```bash
python3 SimpleMultiThreadHTTPServer.py
```

It serves the current project directory at:

```text
http://localhost:8000/
```

Then run the load generator from another terminal:

```bash
bash compile.sh
./http-loadgen http://localhost:8000/ 100
```

Thread-pool example:

```bash
./http-loadgen -p 10 http://localhost:8000/ 100
```

You can also request a specific file served by the Python server:

```bash
./http-loadgen http://localhost:8000/LICENSE 100
```

## Metrics

The tool prints one summary line after each run:

```text
Average-Response-Latency 0.002064 Average-First-Byte-Latency 0.002023 min/max-latency 0.001717/0.002338 requests-per-second 885.832 bytes-per-second 788391 success-rate 100% error-rate 0% status-code-breakdown 200:3 curl-error-breakdown
```

Metric meanings:

- `Average-Response-Latency`: average total request time from libcurl, in seconds
- `Average-First-Byte-Latency`: average time to first byte from libcurl, in seconds
- `min/max-latency`: fastest and slowest total request latency, in seconds
- `requests-per-second`: completed requests divided by total run time
- `bytes-per-second`: response payload bytes received divided by total run time
- `success-rate`: percent of completed requests with curl success and HTTP status `2xx` or `3xx`
- `error-rate`: percent of completed requests counted as failures
- `status-code-breakdown`: HTTP response codes and counts, such as `200:100`
- `curl-error-breakdown`: libcurl failures and counts, such as `Could not connect to server:5`

Small local runs can show very high requests-per-second because `localhost` requests may complete in only a few milliseconds. Use a larger request count for steadier throughput numbers.

## CSV Output

Each run appends a row to:

```text
output.csv
```

Columns:

```text
Average-Response-Latency,Average-First-Byte-Latency,min-latency,max-latency,requests-per-second,bytes-per-second,success-rate,error-rate,success-count,error-count,status-code-breakdown,curl-error-breakdown
```

## Architecture

At a high level, `main` parses command-line arguments, creates a `LoadGeneratorClient`, and calls `Run()`. The client chooses an execution mode, sends requests through libcurl, aggregates shared metrics under a mutex, then prints and saves the results.

Standalone SVG: [docs/diagrams/architecture.svg](docs/diagrams/architecture.svg)

<svg width="760" height="360" viewBox="0 0 760 360" xmlns="http://www.w3.org/2000/svg" role="img" aria-label="HTTP-LoadGenerator architecture">
  <defs>
    <style>
      .box { fill: #f8fafc; stroke: #334155; stroke-width: 2; rx: 8; }
      .mode { fill: #eef6ff; stroke: #2563eb; stroke-width: 2; rx: 8; }
      .metric { fill: #f0fdf4; stroke: #16a34a; stroke-width: 2; rx: 8; }
      .text { font: 14px sans-serif; fill: #0f172a; }
      .title { font: 700 16px sans-serif; fill: #0f172a; }
      .arrow { stroke: #475569; stroke-width: 2; fill: none; marker-end: url(#arrowhead); }
    </style>
    <marker id="arrowhead" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto">
      <polygon points="0 0, 10 3.5, 0 7" fill="#475569" />
    </marker>
  </defs>

  <rect class="box" x="30" y="35" width="145" height="70" />
  <text class="title" x="58" y="65">main()</text>
  <text class="text" x="52" y="88">Parse_Args()</text>

  <rect class="box" x="230" y="35" width="190" height="70" />
  <text class="title" x="255" y="65">LoadGeneratorClient</text>
  <text class="text" x="266" y="88">Run() chooses mode</text>

  <rect class="mode" x="70" y="165" width="210" height="90" />
  <text class="title" x="96" y="195">Thread Per Request</text>
  <text class="text" x="95" y="220">creates one std::thread</text>
  <text class="text" x="116" y="242">per HTTP request</text>

  <rect class="mode" x="345" y="165" width="210" height="90" />
  <text class="title" x="385" y="195">Thread Pool</text>
  <text class="text" x="374" y="220">queues request tasks</text>
  <text class="text" x="366" y="242">onto worker threads</text>

  <rect class="box" x="595" y="165" width="125" height="90" />
  <text class="title" x="626" y="195">libcurl</text>
  <text class="text" x="620" y="220">performs HTTP</text>
  <text class="text" x="622" y="242">and timings</text>

  <rect class="metric" x="230" y="295" width="280" height="50" />
  <text class="title" x="278" y="325">Metrics + output.csv</text>

  <path class="arrow" d="M175 70 L230 70" />
  <path class="arrow" d="M295 105 L190 165" />
  <path class="arrow" d="M355 105 L435 165" />
  <path class="arrow" d="M280 210 L595 210" />
  <path class="arrow" d="M555 210 L595 210" />
  <path class="arrow" d="M655 255 L500 305" />
  <path class="arrow" d="M175 255 L280 305" />
  <path class="arrow" d="M450 255 L425 295" />
</svg>

## Request Flow

Each request follows the same path regardless of execution mode. The mode only changes how the request tasks are scheduled.

Standalone SVG: [docs/diagrams/request-flow.svg](docs/diagrams/request-flow.svg)

<svg width="760" height="320" viewBox="0 0 760 320" xmlns="http://www.w3.org/2000/svg" role="img" aria-label="Request execution flow">
  <defs>
    <style>
      .step { fill: #f8fafc; stroke: #334155; stroke-width: 2; rx: 8; }
      .decision { fill: #fff7ed; stroke: #ea580c; stroke-width: 2; rx: 8; }
      .text { font: 14px sans-serif; fill: #0f172a; }
      .title { font: 700 15px sans-serif; fill: #0f172a; }
      .arrow { stroke: #475569; stroke-width: 2; fill: none; marker-end: url(#arrowhead2); }
    </style>
    <marker id="arrowhead2" markerWidth="10" markerHeight="7" refX="9" refY="3.5" orient="auto">
      <polygon points="0 0, 10 3.5, 0 7" fill="#475569" />
    </marker>
  </defs>

  <rect class="step" x="30" y="55" width="145" height="70" />
  <text class="title" x="56" y="85">Create Task</text>
  <text class="text" x="54" y="108">one per request</text>

  <rect class="step" x="220" y="55" width="145" height="70" />
  <text class="title" x="242" y="85">curl_easy_init</text>
  <text class="text" x="251" y="108">configure URL</text>

  <rect class="step" x="410" y="55" width="145" height="70" />
  <text class="title" x="432" y="85">Perform HTTP</text>
  <text class="text" x="438" y="108">curl_easy_perform</text>

  <rect class="step" x="595" y="55" width="135" height="70" />
  <text class="title" x="617" y="85">Collect Info</text>
  <text class="text" x="616" y="108">latency + code</text>

  <rect class="decision" x="140" y="205" width="180" height="70" />
  <text class="title" x="172" y="235">Classify Result</text>
  <text class="text" x="160" y="258">success or error</text>

  <rect class="step" x="385" y="205" width="180" height="70" />
  <text class="title" x="416" y="235">Aggregate Metrics</text>
  <text class="text" x="415" y="258">protected by mutex</text>

  <rect class="step" x="610" y="205" width="120" height="70" />
  <text class="title" x="637" y="235">Report</text>
  <text class="text" x="624" y="258">stdout + CSV</text>

  <path class="arrow" d="M175 90 L220 90" />
  <path class="arrow" d="M365 90 L410 90" />
  <path class="arrow" d="M555 90 L595 90" />
  <path class="arrow" d="M660 125 L285 205" />
  <path class="arrow" d="M320 240 L385 240" />
  <path class="arrow" d="M565 240 L610 240" />
</svg>

## Code Layout

- `LoadGeneratorClient.cpp` / `LoadGeneratorClient.hpp`: command-line parsing, execution mode selection, libcurl request handling, metric aggregation, stdout and CSV output
- `ThreadPool.cpp` / `ThreadPool.hpp`: fixed-size worker pool used by `-p <pool-size>`
- `SimpleMultiThreadHTTPServer.py`: local threaded Python HTTP server for testing
- `compile.sh`: simple build script for `http-loadgen`
- `output.csv`: generated metrics file

## Notes and Limitations

- The tool currently sends `GET` requests only.
- HTTPS depends on the libcurl build available on your system.
- Metrics are process-global static fields, so each process run should execute one load test.
- `requests-per-second` is a measured rate: `completed requests / total elapsed run time`. It can be much larger than `<request-count>` for very fast local tests.
