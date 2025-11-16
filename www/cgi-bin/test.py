#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys

# CGI ヘッダを出力
print("Content-Type: text/html")
print()

# HTML 出力
print("<!DOCTYPE html>")
print("<html>")
print("<head><title>Python CGI Test</title></head>")
print("<body>")
print("<h1>Python CGI Test</h1>")
print("<h2>Environment Variables:</h2>")
print("<ul>")

# 主要な CGI 環境変数を表示
env_vars = [
    'REQUEST_METHOD',
    'SCRIPT_FILENAME',
    'QUERY_STRING',
    'CONTENT_LENGTH',
    'CONTENT_TYPE',
    'SERVER_PROTOCOL',
    'SERVER_NAME',
    'PATH_INFO',
    'SCRIPT_NAME'
]

for var in env_vars:
    value = os.environ.get(var, '(not set)')
    print(f"<li><strong>{var}:</strong> {value}</li>")

print("</ul>")

# POST データがあれば表示
if os.environ.get('REQUEST_METHOD') == 'POST':
    content_length = int(os.environ.get('CONTENT_LENGTH', 0))
    if content_length > 0:
        post_data = sys.stdin.read(content_length)
        print("<h2>POST Data:</h2>")
        print(f"<pre>{post_data}</pre>")

print("</body>")
print("</html>")
