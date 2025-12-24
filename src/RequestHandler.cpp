#include "RequestHandler.hpp"

namespace {

// Converts a value to a string using stringstream.
// This is a C++98 compatible alternative to std::to_string.
//
// Args:
//   value: The value to convert.
//
// Returns:
//   The string representation of the value.
template <typename T>
std::string toString(const T& value) {
  std::ostringstream oss;
  oss << value;
  if (oss.fail()) {
    return "";
  }
  return oss.str();
}

// Normalizes the URI to prevent path traversal attacks.
// Resolves segments like "/../" to ensure the path does not traverse above the root directory.
//
// Args:
//   uri: The raw URI string to normalize.
//
// Returns:
//   The normalized URI string.
static std::string normalizeUri(const std::string& uri) {
  std::vector<std::string> parts;
  std::string::size_type start = 0;
  std::string::size_type end;

  while ((end = uri.find('/', start)) != std::string::npos) {
    std::string part = uri.substr(start, end - start);
    if (!part.empty() && part != ".") {
      if (part == "..") {
        if (!parts.empty())
          parts.pop_back();
      } else {
        parts.push_back(part);
      }
    }
    start = end + 1;
  }
  if (start < uri.length()) {
    std::string part = uri.substr(start);
    if (!part.empty() && part != ".") {
      if (part == "..") {
        if (!parts.empty())
          parts.pop_back();
      } else {
        parts.push_back(part);
      }
    }
  }

  if (parts.empty()) {
    return "/";
  }

  std::string normalized;
  for (std::vector<std::string>::const_iterator it = parts.begin();
       it != parts.end(); ++it) {
    normalized += "/" + (*it);
  }

  if (uri.size() > 1 && *(uri.end() - 1) == '/') {
    normalized += "/";
  }
  return normalized;
}

// Formats a time_t value into a readable string (e.g., "01-Jan-2023 12:00").
// Implemented manually to comply with restrictions on using C standard library time functions.
//
// Args:
//   timer: The time_t value (seconds since epoch).
//
// Returns:
//   A formatted date-time string.
std::string formatTime(time_t timer) {
  long seconds = static_cast<long>(timer);
  const long secPerMin = 60;
  const long secPerHour = 3600;
  const long secPerDay = 86400;

  long days = seconds / secPerDay;
  long remSeconds = seconds % secPerDay;

  if (remSeconds < 0) {
    remSeconds += secPerDay;
    days--;
  }

  long hour = remSeconds / secPerHour;
  remSeconds %= secPerHour;
  long min = remSeconds / secPerMin;

  int year = 1970;
  if (days >= 0) {
    while (true) {
      bool isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
      int daysInYear = isLeap ? 366 : 365;
      if (days < daysInYear)
        break;
      days -= daysInYear;
      year++;
    }
  } else {
    while (days < 0) {
      bool isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
      int daysInYear = isLeap ? 366 : 365;
      days += daysInYear;
      year--;
    }
  }

  int month = 0;
  bool isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
  int daysInMonth[12] = {
      31, (isLeap ? 29 : 28), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  static const char* monthNames[12] = {"Jan", "Feb", "Mar", "Apr",
                                       "May", "Jun", "Jul", "Aug",
                                       "Sep", "Oct", "Nov", "Dec"};
  while (days >= daysInMonth[month]) {
    days -= daysInMonth[month];
    month++;
  }

  int day = days += 1;

  std::ostringstream oss;
  oss << std::setw(2) << std::setfill('0') << day << "-" << monthNames[month]
      << "-" << year << " " << std::setw(2) << std::setfill('0') << hour << ":"
      << std::setw(2) << std::setfill('0') << min;

  if (oss.fail()) {
    return "";
  }
  return oss.str();
}

struct FileEntry {
  std::string name;
  bool isDir;
  time_t mtime;
  off_t size;

  static bool compare(const FileEntry& a, const FileEntry& b) {
    if (a.isDir != b.isDir) {
      return (a.isDir);
    }
    return a.name < b.name;
  }
};

// Collects file entries from a directory and sorts them.
//
// Args:
//   dirPath: The path to the directory to read.
//   entries: A vector to store the collected FileEntry objects.
//
// Returns:
//   true if successful, false if the directory could not be opened.
bool collectFileEntries(const std::string& dirPath,
                        std::vector<FileEntry>& entries) {
  DIR* dir = opendir(dirPath.c_str());
  if (!dir) {
    return false;
  }
  struct dirent* entry;
  while ((entry = readdir(dir))) {
    std::string name = entry->d_name;
    if (name == ".")
      continue;

    std::string fullPath = dirPath;
    if (!fullPath.empty() && *(fullPath.end() - 1) != '/') {
      fullPath += "/";
    }
    fullPath += name;

    struct stat fileStat;
    if (stat(fullPath.c_str(), &fileStat) == 0) {
      FileEntry e;
      e.name = name;
      e.isDir = S_ISDIR(fileStat.st_mode);
      e.mtime = fileStat.st_mtime;
      e.size = fileStat.st_size;
      entries.push_back(e);
    }
  }
  closedir(dir);
  std::sort(entries.begin(), entries.end(), FileEntry::compare);
  return true;
}

// Formats the file size for display.
// Returns "-" for directories.
//
// Args:
//   e: The FileEntry object.
//
// Returns:
//   The formatted size string.
std::string formatSize(const FileEntry& e) {
  if (e.isDir) {
    return "-";
  }
  return toString(e.size);
}

// Generates the HTML content for the autoindex page.
//
// Args:
//   entries: A list of file entries to display.
//   requestUri: The URI of the request (used for title and header).
//   outHtml: Output parameter to store the generated HTML.
//
// Returns:
//   true on success, false on failure (e.g., stream error).
bool generateAutoIndexHtml(const std::vector<FileEntry>& entries,
                           const std::string& requestUri,
                           std::string& outHtml) {
  std::ostringstream htmlOss;

  htmlOss << "<html>\r\n"
          << "<head><title>Index of " << requestUri << "</title></head>\r\n"
          << "<body>\r\n"
          << "<h1>Index of " << requestUri << "</h1>\r\n"
          << "<hr><pre>\r\n";
  htmlOss << std::left << std::setw(50) << "Name" << std::setw(25)
          << "Last modified" << std::right << std::setw(15) << "Size" << "\r\n";
  htmlOss << "<hr>\r\n";

  for (std::vector<FileEntry>::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    std::string name = it->name;
    std::string displayName = name + (it->isDir ? "/" : "");

    // 表示用の省略名作成
    std::string linkName = displayName;
    if (linkName.length() > 45) {
      linkName = linkName.substr(0, 42) + "..>";
    }

    std::string timeStr = formatTime(it->mtime);
    if (timeStr.empty()) {
      return false;
    }

    std::string sizeStr = formatSize(*it);
    if (sizeStr.empty()) {
      return false;
    }

    // HTML行生成
    htmlOss << "<a href=\"" << displayName << "\">" << std::left
            << std::setw(50) << linkName << "</a> " << std::setw(25) << timeStr
            << std::right << std::setw(15) << sizeStr << "\r\n";
  }

  htmlOss << "</pre><hr></body>\r\n</html>";

  if (htmlOss.fail()) {
    return false;
  }
  outHtml = htmlOss.str();
  return true;
}

// Determines the target file path for upload.
// Uses upload_path if available, otherwise uses the resolved real path.
//
// Args:
//   req: The HTTP request object.
//   realPath: The resolved file system path.
//   location: The matched LocationConfig.
//
// Returns:
//   The determined file system path for saving the file.
std::string resolveUploadPath(const HttpRequest& req,
                              const std::string& realPath,
                              const LocationConfig* location) {
  if (location && !location->upload_path.empty()) {
    std::string targetPath = location->upload_path;
    if (!targetPath.empty() && *targetPath.rbegin() != '/') {
      targetPath += "/";
    }

    std::string uri = req.getPath();
    std::string filename;
    size_t lastSlash = uri.rfind('/');
    if (lastSlash != std::string::npos && lastSlash + 1 < uri.size()) {
      filename = uri.substr(lastSlash + 1);
    } else {
      filename = "default_upload.dat";
    }
    return targetPath + filename;
  }
  return realPath;
}

// Writes data to a file, overwriting if it exists.
//
// Args:
//   path: The file path to write to.
//   data: The content to write.
//
// Returns:
//   0 on success, or an HTTP status code (403, 404, 500) on failure.
int writeFile(const std::string& path, const std::vector<char>& data) {
  std::ofstream ofs(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!ofs.is_open()) {
    if (errno == EACCES)
      return 403;  // Forbidden
    if (errno == ENOENT)
      return 404;  // Parent directory missing
    return 500;    // Internal Error
  }
  if (!data.empty()) {
    ofs.write(&data[0], data.size());
  }
  if (ofs.bad()) {
    ofs.close();
    return 500;
  }
  ofs.close();
  return 0;
}

int removeFile(const std::string& path) {
  if (unlink(path.c_str()) == 0)
    return (0);
  if (errno == EACCES || errno == EPERM)
    return 403;  // Forbidden
  if (errno == ENOENT)
    return 404;  // Not Found
  return 500;    // Internal Error
}

}  // namespace

RequestHandler::RequestHandler(const MainConfig& config) : _config(config) {}

RequestHandler::~RequestHandler() {}

// Main entry point for handling client requests.
// Analyzes the request, identifies the appropriate configuration, resolve paths,
// and delegates processing to specific method handlers.
// Supports internal redirection for error handling.
//
// Args:
//   client: Pointer for the Client object holding request and response data.
void RequestHandler::handle(Client* client) {
  if (!client)
    return;

  int redirectCount = 0;
  const int maxRedirects = 10;
  int finalStatusCode = 0;
  std::string prevUri;

  while (redirectCount++ < maxRedirects) {
    const HttpRequest& req = client->req;
    std::string currentUri = req.getPath();

    if (!prevUri.empty() && currentUri == prevUri) {
      client->res.makeErrorResponse(508, NULL);
      client->readyToWrite();
      return;
    }
    prevUri = currentUri;

    const ServerConfig* matchedServer = _findServerConfig(client);
    if (!matchedServer) {
      _handleError(client, 500);
      return;
    }

    const LocationConfig* matchedLocation =
        _findLocationConfig(client->req, *matchedServer);

    if (matchedLocation && matchedLocation->return_redirect.first != 0) {
      _handleRedirection(client, matchedLocation);
      return;
    }

    std::string realPath =
        _resolvePath(client->req.getPath(), *matchedServer, matchedLocation);

    if (matchedLocation) {
      const std::vector<HttpMethod>& allowed = matchedLocation->allow_methods;
      if (std::find(allowed.begin(), allowed.end(), req.getMethod()) ==
          allowed.end()) {
        if (_handleError(client, 405)) {
          continue;
        }  // Method not allowed
        return;
      }
    }

    int procResult = 0;
    switch (req.getMethod()) {
      case GET:
        procResult = _handleGet(client, realPath, matchedLocation);
        break;
      case POST:
        procResult = _handlePost(client, realPath, matchedLocation);
        break;
      case DELETE:
        procResult = _handleDelete(client, realPath, matchedLocation);
        break;
      default:
        procResult = 405;
        break;
    }
    if (procResult == 0) {
      if (finalStatusCode != 0) {
        client->res.setStatusCode(finalStatusCode);
      }
      return;
    }
    if (finalStatusCode == 0) {
      finalStatusCode = procResult;
    }
    if (_handleError(client, procResult)) {
      continue;
    }
    return;
  }
  client->res.makeErrorResponse(500, NULL);  // internal server error
  client->readyToWrite();
}

const ServerConfig* RequestHandler::_findServerConfig(const Client* client) {
  return _config.getServer(client->req.getHeader("Host"),
                           client->getListenPort());
}

const LocationConfig* RequestHandler::_findLocationConfig(
    const HttpRequest& req, const ServerConfig& serverConfig) {
  std::string uri = normalizeUri(req.getPath());
  return serverConfig.getLocation(uri);
}

std::string RequestHandler::_resolvePath(const std::string& uri,
                                         const ServerConfig& serverConfig,
                                         const LocationConfig* location) {
  std::string normalizedUri = normalizeUri(uri);
  if (!location) {
    return serverConfig.root + normalizedUri;
  }
  std::string path = normalizedUri;
  if (!location->alias.empty() &&
      path.compare(0, location->path.length(), location->path) == 0) {
    path.replace(0, location->path.length(), location->alias);
  } else {
    std::string root =
        location->root.empty() ? serverConfig.root : location->root;
    path = root + normalizedUri;
  }
  return path;
}

// Handle GET requests.
// Checks for file existence, permissions, and searches for index files if the path is a directory.
//
// Args:
//   client: Pointer to the Client object.
//   realPath: The resolved file system path.
//   location: The matched LocationConfig.
int RequestHandler::_handleGet(Client* client, const std::string& realPath,
                               const LocationConfig* location) {
  std::string pathToFile = realPath;
  if (!_isFileExist(pathToFile)) {
    return 404;  // Not found
  }
  if (_isDirectory(pathToFile)) {
    std::string indexFile = "index.html";
    if (location && !location->index.empty()) {
      indexFile = location->index;
    }
    std::string candidatePath = pathToFile;
    if (!candidatePath.empty() && *(candidatePath.end() - 1) != '/') {
      candidatePath += "/";
    }
    candidatePath += indexFile;
    if (_isFileExist(candidatePath)) {
      pathToFile = candidatePath;
    } else if (location && location->autoindex) {
      _generateAutoIndex(client, pathToFile);
      return 0;
    } else {
      return 403;  // Forbidden
    }
  }
  if (_isDirectory(pathToFile)) {
    return 403;  // Forbidden
  }
  if (!_checkPermission(pathToFile, "r")) {
    return 403;  // Forbidden
  }
  if (client->res.setBodyFile(pathToFile)) {
    client->res.setStatusCode(200);
    client->readyToWrite();
    return 0;
  } else {
    return 500;  // Internal Server Error
  }
}

// Handles POST requests.
// Writes the request body to a file. Supports upload_path if configured.
//
// Args:
//   client: Pointer to the Client object.
//   realPath: The resolved file system path.
//   location: The matched LocationConfig.
//
// Returns:
//   HTTP status code (0 for success, or error code).
int RequestHandler::_handlePost(Client* client, const std::string& realPath,
                                const LocationConfig* location) {
  std::string targetPath = resolveUploadPath(client->req, realPath, location);
  if (_isDirectory(targetPath)) {
    return 403;  // Forbidden;
  }
  int writeResult = writeFile(targetPath, client->req.getBody());
  if (writeResult != 0)
    return writeResult;

  client->res.setStatusCode(201);  // Created
  client->res.setHeader("Location", client->req.getPath());
  client->res.setBody("Created");
  client->readyToWrite();

  return 0;
}

// Handles DELETE requests by removing the specified resource.
//
// Args:
//   client: Pointer to the Client object.
//   realPath: The resolved file system path.
//   location: The matched LocationConfig.
//
// Returns:
//   0 on success, or an HTTP status code on failure.
int RequestHandler::_handleDelete(Client* client, const std::string& realPath,
                                  const LocationConfig* location) {
  (void)location;
  if (!_isFileExist(realPath)) {
    return 404;  // Not found
  }
  if (_isDirectory(realPath)) {
    return 403;  // Forbidden
  }
  if (!_checkPermission(realPath, "w")) {
    return 403;  // Forbidden
  }

  int removeResult = removeFile(realPath);
  if (removeResult != 0) {
    return removeResult;
  }
  client->res.setStatusCode(204);  // No Content
  client->readyToWrite();
  return 501;
}

void RequestHandler::_generateAutoIndex(Client* client,
                                        const std::string& dirPath) {
  std::vector<FileEntry> entries;
  if (!collectFileEntries(dirPath, entries)) {
    _handleError(client, 403);  // Forbidden
    return;
  }

  std::string htmlContent;
  if (!generateAutoIndexHtml(entries, client->req.getPath(), htmlContent)) {
    _handleError(client, 500);  // Internal server error
    return;
  }

  client->res.setStatusCode(200);
  client->res.setHeader("Content-Type", "text/html");
  client->res.setBody(htmlContent);
  client->readyToWrite();
}

// Handle HTTP redirection specified in the Location configulation.
// Sets the status code and Location header.
// Only allow valid redirection codes: 301, 302, 303, 307, 308.
// If invalid, fallback to 302 (Found).
//
// Args:
//   client: Pointer to the Client object.
//   location: The matched LocationConfig containing redirection details.
void RequestHandler::_handleRedirection(Client* client,
                                        const LocationConfig* location) {
  int code = location->return_redirect.first;
  const std::string& uri = location->return_redirect.second;
  switch (code) {
    case 301:
    case 302:
    case 303:
    case 307:
    case 308:
      break;
    default:
      code = 302;
      break;
  }
  client->res.makeErrorResponse(code, NULL);
  client->res.setHeader("Location", uri);
  client->readyToWrite();
}

// Handles errors by checking for custom error pages or generating a default response.
// Supports internal redirection if a custom error page is configured.
//
// Args:
//   client: Pointer to the Client object.
//   statusCode: The HTTP status code indicating the error.
//
// Returns:
//   true if an internal redirection occurred, false otherwise.
bool RequestHandler::_handleError(Client* client, int statusCode) {
  const ServerConfig* serverConfig = client->req.getConfig();
  if (!serverConfig) {
    serverConfig = _findServerConfig(client);
  }
  if (serverConfig && serverConfig->error_pages.count(statusCode) > 0) {
    std::string errorUri = serverConfig->error_pages.at(statusCode);
    if (!errorUri.empty() && *errorUri.begin() == '/') {
      client->req.setPath(errorUri);
      return true;
    }
  }
  client->res.makeErrorResponse(statusCode, NULL);
  client->readyToWrite();
  return false;
}

// Determines if the specified path is a directory.
//
// Args:
//   path: The path to check.
//
// Returns:
//   true if it is a directory, false otherwise.
bool RequestHandler::_isDirectory(const std::string& path) {
  struct stat buffer;
  if (stat(path.c_str(), &buffer) != 0) {
    if (errno != ENOENT) {
      std::cerr << "[Warn] _isDirectory: stat failed for " << path << "("
                << strerror(errno) << ")" << std::endl;
    }
    return false;
  }
  return S_ISDIR(buffer.st_mode);
}

// Check if a file or a directory exists at the specified path.
// Args:
//   path: the path to check.
//
// Returns:
//   true if it exists, false otherwise.
bool RequestHandler::_isFileExist(const std::string& path) {
  if (access(path.c_str(), F_OK) == 0)
    return true;
  if (errno != ENOENT) {
    std::cerr << "[Warn] _isFileExist: access() failed for " << path << "("
              << strerror(errno) << ")" << std::endl;
  }
  return false;
}

// Checks if there is access permission for the specified path with the specified mode.
//
// Args:
//   path: The path to check.
//   mode: The permission mode to check ("r" for read, "w" for write,  and "x" for execute.)
//
// Returns:
//   true if permission is granted, false otherwise.
bool RequestHandler::_checkPermission(const std::string& path,
                                      const std::string& mode) {
  int modeFlag = R_OK;
  if (mode == "w")
    modeFlag = W_OK;
  else if (mode == "x")
    modeFlag = X_OK;

  if (access(path.c_str(), modeFlag) == 0) {
    return true;
  }

  if (errno != EACCES) {
    std::cerr << "[Warn] _checkPermission: access() failed for " << path << "("
              << strerror(errno) << ")" << std::endl;
  }

  return false;
}
