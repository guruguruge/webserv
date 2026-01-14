#!/usr/bin/env python3
import os

print("Content-Type: text/html")
print()
print("<html><body>")
print("<h1>CGI Test</h1>")
print("<p>Query String: {}</p>".format(os.environ.get('QUERY_STRING', '')))
print("<p>Request Method: {}</p>".format(os.environ.get('REQUEST_METHOD', '')))
print("</body></html>")
