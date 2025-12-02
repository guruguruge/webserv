#include "Http.hpp"

HttpResponse::HttpResponse() : _statusCode(200), _statusMessage("OK"), _sentBytes(0) {};

HttpResponse::~HttpResponse() {};

HttpResponse::HttpResponse(const HttpResponse &other) { *this = other; }

HttpResponse &HttpResponse::operator=(const HttpResponse &other) {
    if (this != &other) {
        this->_statusCode     = other._statusCode;
        this->_statusMessage  = other._statusMessage;
        this->_headers        = other._headers;
        this->_body           = other._body;
        this->_responseBuffer = other._responseBuffer;
        this->_sentBytes      = other._sentBytes;
    }
    return (*this);
}

void HttpResponse::clear() {
    this->_statusCode    = 200;
    this->_statusMessage = "OK";
    this->_headers.clear();
    this->_body.clear();
    this->_responseBuffer.clear();
    this->_sentBytes = 0;
}

void HttpResponse::setStatusCode(int code) {
    this->_statusCode = code;
    static std::map<int, std::string> statusMap;
    if (statusMap.empty()) {
        statusMap[200] = "OK";
        statusMap[201] = "Created";
        statusMap[204] = "No Content";
        statusMap[301] = "Moved Permanently";
        statusMap[302] = "Found";
        statusMap[400] = "Bad Request";
        statusMap[401] = "Unauthorized";
        statusMap[403] = "Forbidden";
        statusMap[404] = "Not Found";
        statusMap[405] = "Method Not Allowed";
        statusMap[500] = "Internal Server Error";
        statusMap[501] = "Not Implemented";
        statusMap[502] = "Bad Gateway";
        statusMap[503] = "Service Unavailable";
    }
    if (statusMap.count(code)) {
        _statusMessage = statusMap[code];
    } else {
        _statusMessage = "Unknown Status";
    }
}

void HttpResponse::setHeader(const std::string &key, const std::string &value) { _headers[key] = value; }

void HttpResponse::setBody(const std::string &body) { _body.assign(body.begin(), body.end()); }

void HttpResponse::setBody(const std::vector<char> &body) { _body = body; }

void HttpResponse::setBodyFile(const std::string &filepath) {
    (void)filepath;  // 未使用変数の警告消し
    // TODO: PR4で実装
}

void HttpResponse::makeErrorResponse(int code, const ServerConfig *config) {
    (void)code;
    (void)config;
    // TODO: PR5で実装
}

void HttpResponse::build() {
    // TODO: PR2で実装
}

const char *HttpResponse::getData() const {
    // TODO: PR3で実装
    return NULL;
}

size_t HttpResponse::getRemainingSize() const {
    // TODO: PR3で実装
    return 0;
}

void HttpResponse::advance(size_t n) {
    (void)n;
    // TODO: PR3で実装
}

bool HttpResponse::isDone() const {
    // TODO: PR3で実装
    return true;
}