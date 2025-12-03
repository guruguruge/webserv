#ifndef UPLOADHANDLER_HPP
#define UPLOADHANDLER_HPP

#include <string>
#include "Config.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class UploadHandler {
 public:
  static HttpResponse handlePost(const HttpRequest& req,
                                 const ServerConfig& server,
                                 const LocationConfig* location);

 private:
  static HttpResponse handleMultipart(const HttpRequest& req,
                                      const LocationConfig* location);
  static HttpResponse handleRaw(const HttpRequest& req,
                                const LocationConfig* location);

  static std::string extractBoundary(const std::string& contentType);
  static std::string extractFilename(const std::string& contentDisposition);
  static std::string generateUniqueFilename();
  static bool saveFile(const std::string& path, const std::string& content);
};

#endif
