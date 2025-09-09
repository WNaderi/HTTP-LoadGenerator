# threaded_server.py
from http.server import HTTPServer, SimpleHTTPRequestHandler
from socketserver import ThreadingMixIn

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    pass

if __name__ == '__main__':
    server = ThreadedHTTPServer(('localhost', 8000), SimpleHTTPRequestHandler)
    print("Serving on port 8000...")
    server.serve_forever()
