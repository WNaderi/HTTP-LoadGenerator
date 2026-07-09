# HTTP-LoadGenerator
Multithreaded HTTP load generator command-line tool.

## Compile

```bash
bash compile.sh
```

## Run

Thread per request mode:

```bash
./http-loadgen <url> <request-count>
```

Thread pool mode:

```bash
./http-loadgen -p <pool-size> <url> <request-count>
```

Example with the local Python server:

```bash
python3 SimpleMultiThreadHTTPServer.py
./http-loadgen http://localhost:8000/ 100
./http-loadgen -p 10 http://localhost:8000/ 100
```
