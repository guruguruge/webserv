#include "Http.hpp"

std::string HttpResponse::getMimeType(const std::string& filepath) {
  static std::map<std::string, std::string> mimeTypes;
  if (mimeTypes.empty()) {
    // --- Text ---
    mimeTypes[".html"] = "text/html";
    mimeTypes[".htm"] = "text/html";
    mimeTypes[".css"] = "text/css";
    mimeTypes[".js"] = "text/javascript";
    mimeTypes[".txt"] = "text/plain";
    mimeTypes[".csv"] = "text/csv";

    // --- Image ---
    mimeTypes[".jpg"] = "image/jpeg";
    mimeTypes[".jpeg"] = "image/jpeg";
    mimeTypes[".png"] = "image/png";
    mimeTypes[".gif"] = "image/gif";
    mimeTypes[".bmp"] = "image/bmp";
    mimeTypes[".svg"] = "image/svg+xml";
    mimeTypes[".ico"] = "image/x-icon";

    // --- Application ---
    mimeTypes[".pdf"] = "application/pdf";
    mimeTypes[".zip"] = "application/zip";
    mimeTypes[".tar"] = "application/x-tar";
    mimeTypes[".json"] = "application/json";

    // --- Audio/Video ---
    mimeTypes[".mp3"] = "audio/mpeg";
    mimeTypes[".mp4"] = "video/mp4";

    // --- Office (必要であれば) ---
    mimeTypes[".xls"] = "application/vnd.ms-excel";
    mimeTypes[".xlsx"] =
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    mimeTypes[".doc"] = "application/msword";
    mimeTypes[".docx"] =
        "application/"
        "vnd.openxmlformats-officedocument.wordprocessingml.document";
    mimeTypes[".ppt"] = "application/vnd.ms-powerpoint";
    mimeTypes[".pptx"] =
        "application/"
        "vnd.openxmlformats-officedocument.presentationml.presentation";
  }

  std::string::size_type n;
  n = filepath.rfind(".");
  if (n != std::string::npos) {
    std::string ext = filepath.substr(n);
    if (mimeTypes.count(ext))
      return mimeTypes[ext];
  }
  return "application/octet-stream";
}

HttpResponse::HttpResponse()
    : _statusCode(200), _statusMessage("OK"), _sentBytes(0) {}

HttpResponse::~HttpResponse() {}

HttpResponse::HttpResponse(const HttpResponse& other)
    : _statusCode(other._statusCode),
      _statusMessage(other._statusMessage),
      _headers(other._headers),
      _body(other._body),
      _responseBuffer(other._responseBuffer),
      _sentBytes(other._sentBytes) {}

HttpResponse& HttpResponse::operator=(const HttpResponse& other) {
  if (this != &other) {
    this->_statusCode = other._statusCode;
    this->_statusMessage = other._statusMessage;
    this->_headers = other._headers;
    this->_body = other._body;
    this->_responseBuffer = other._responseBuffer;
    this->_sentBytes = other._sentBytes;
  }
  return (*this);
}

void HttpResponse::clear() {
  this->_statusCode = 200;
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

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
  _headers[key] = value;
}

void HttpResponse::setBody(const std::string& body) {
  _body.assign(body.begin(), body.end());
}

void HttpResponse::setBody(const std::vector<char>& body) {
  _body = body;
}

// Reads file and fill _body. if there is no "Content-Type" in _headers, sets "Content-Type" based on extension.
// inputs:
//   filepath: input file's filepath
// returns:
//   bool: false when filepath is invalid or error occurs while reading, otherwise true
bool HttpResponse::setBodyFile(const std::string& filepath) {
  std::ifstream ifs(filepath.c_str(), std::ios_base::in |
                                          std::ios_base::binary |
                                          std::ios_base::ate);
  if (!ifs.is_open()) {
    return (false);
  }

  // adjust _body size to the file size
  std::ifstream::pos_type endPos = ifs.tellg();
  if (endPos == std::ifstream::pos_type(-1)) {
    return (false);
  }
  std::size_t size = static_cast<std::size_t>(endPos);
  ifs.seekg(0, std::ios::beg);

  std::vector<char> tmpBody;
  tmpBody.resize(size);
  if (size) {
    ifs.read(&tmpBody[0], size);
    if (ifs.fail())
      return (false);
  }
  this->_body.swap(tmpBody);

  // if there is no content-type in headers, sets extension automatically.
  if (!this->_headers.count("Content-Type"))
    this->_headers["Content-Type"] = getMimeType(filepath);

  return (true);
}

void HttpResponse::makeErrorResponse(int code, const ServerConfig* config) {
  (void)code;
  (void)config;
  // TODO: PR5で実装
}

// builds http response(status line, response header, response body) based on its attributes.
// status line: "HTTP/1.0 <status code> <status message>\r\n"
// response header: "key: value\r\n" iteration
// response body: body content
void HttpResponse::build() {
  this->_responseBuffer.clear();
  this->_sentBytes = 0;

  // if headers has no "Content-Length", calculates body size and '"Content-Length": <body size>' pair.
  if (!this->_headers.count("Content-Length")) {
    std::ostringstream len_ss;
    len_ss << this->_body.size();
    this->_headers["Content-Length"] = len_ss.str();
  }

  // creates status line and response header string
  std::ostringstream ss;
  ss << "HTTP/1.0 " << this->_statusCode << " " << this->_statusMessage
     << "\r\n";
  for (std::map<std::string, std::string>::iterator it = this->_headers.begin();
       it != this->_headers.end(); ++it) {
    ss << it->first << ": " << it->second << "\r\n";
  }
  ss << "\r\n";

  // insert status line and response header string to buffer
  std::string status_line_and_header = ss.str();
  this->_responseBuffer.insert(this->_responseBuffer.end(),
                               status_line_and_header.begin(),
                               status_line_and_header.end());

  // insert response body to buffer
  if (!this->_body.empty())
    this->_responseBuffer.insert(this->_responseBuffer.end(),
                                 this->_body.begin(), this->_body.end());
}

const char* HttpResponse::getData() const {
  if (this->_responseBuffer.empty() ||
      this->_sentBytes >= this->_responseBuffer.size())
    return (NULL);
  return (&this->_responseBuffer[this->_sentBytes]);
}

size_t HttpResponse::getRemainingSize() const {
  if (this->_sentBytes >= this->_responseBuffer.size()) {
    return (0);
  } else {
    return (this->_responseBuffer.size() - this->_sentBytes);
  }
}

void HttpResponse::advance(size_t n) {
  if (this->_sentBytes >= this->_responseBuffer.size() ||
      this->_responseBuffer.size() - this->_sentBytes < n) {
    this->_sentBytes = this->_responseBuffer.size();
  } else {
    this->_sentBytes += n;
  }
}

bool HttpResponse::isDone() const {
  return (this->_sentBytes >= this->_responseBuffer.size());
}
