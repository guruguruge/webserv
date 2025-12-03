#include "HttpResponse.hpp"
#include <sstream>

HttpResponse::HttpResponse()
    : _statusCode(200), _reasonPhrase("OK"), _body("") {}

HttpResponse::~HttpResponse() {}

void HttpResponse::setStatusCode(int code) {
  _statusCode = code;
  _reasonPhrase = getDefaultReasonPhrase(code);
}

int HttpResponse::getStatusCode() const {
  return _statusCode;
}

void HttpResponse::setReasonPhrase(const std::string& phrase) {
  _reasonPhrase = phrase;
}

const std::string& HttpResponse::getReasonPhrase() const {
  return _reasonPhrase;
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
  _headers[key] = value;
}

void HttpResponse::setBody(const std::string& body) {
  _body = body;
}

void HttpResponse::appendBody(const std::string& data) {
  _body.append(data);
}

const std::string& HttpResponse::getBody() const {
  return _body;
}

std::string HttpResponse::serialize() const {
  std::ostringstream oss;

  // ステータスライン
  oss << "HTTP/1.1 " << _statusCode << " " << _reasonPhrase << "\r\n";

  // ヘッダーフィールド
  for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
       it != _headers.end(); ++it) {
    oss << it->first << ": " << it->second << "\r\n";
  }

  // 空行
  oss << "\r\n";

  // ボディ
  oss << _body;

  return oss.str();
}

std::string HttpResponse::getDefaultReasonPhrase(int statusCode) {
  switch (statusCode) {
    // 2xx Success
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 204:
      return "No Content";

    // 3xx Redirection
    case 301:
      return "Moved Permanently";
    case 302:
      return "Found";
    case 304:
      return "Not Modified";

    // 4xx Client Error
    case 400:
      return "Bad Request";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 413:
      return "Payload Too Large";

    // 5xx Server Error
    case 500:
      return "Internal Server Error";
    case 501:
      return "Not Implemented";
    case 502:
      return "Bad Gateway";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
    case 505:
      return "HTTP Version Not Supported";

    default:
      return "Unknown";
  }
}
