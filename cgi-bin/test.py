#!/usr/bin/env python3
import os
import datetime

print("Content-Type: text/html")
print()
print("<!DOCTYPE html>")
print("<html>")
print("<head>")
print("    <title>CGI Test</title>")
print("    <style>")
print(
    "        body { font-family: Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; }"
)
print(
    "        .info { background-color: #e8f4f8; padding: 15px; margin: 10px 0; border-left: 4px solid #2196f3; }"
)
print("        h1 { color: #2c3e50; }")
print(
    "        code { background-color: #f4f4f4; padding: 2px 5px; border-radius: 3px; }"
)
print("    </style>")
print("</head>")
print("<body>")
print("    <h1>🐍 CGI Test - Python</h1>")
print("    <div class='info'>")
print("        <p><strong>Status:</strong> CGI is working correctly!</p>")
print(f"        <p><strong>Current Time:</strong> {datetime.datetime.now()}</p>")
print("    </div>")
print("    <h2>Environment Variables</h2>")
print("    <div class='info'>")

env_vars = [
    "REQUEST_METHOD",
    "QUERY_STRING",
    "CONTENT_TYPE",
    "CONTENT_LENGTH",
    "SERVER_NAME",
    "SERVER_PORT",
    "SCRIPT_NAME",
    "PATH_INFO",
]

for var in env_vars:
    value = os.environ.get(var, "(not set)")
    print(f"        <p><code>{var}</code>: {value}</p>")

print("    </div>")
print("    <p><a href='/'>← Back to Home</a></p>")
print("</body>")
print("</html>")
