#include "RequestHandler.hpp"

RequestHandler::RequestHandler(const MainConfig& config) : _config(config) {}

RequestHandler::~RequestHandler() {}

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
  for (size_t i = 0; i < parts.size(); ++i) {
    normalized += "/" + parts[i];
  }
  return normalized;
}

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

  while (redirectCount++ < maxRedirects) {
    const HttpRequest& req = client->req;

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
  client->setState(WRITING_RESPONSE);
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
    } else {
      // 本来はここでAutoIndexの判定を行うが、今回は未実装のため403 Forbidden
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

int RequestHandler::_handlePost(Client* client, const std::string& realPath,
                                const LocationConfig* location) {
  (void)client;
  (void)realPath;
  (void)location;
  return 501;
}

int RequestHandler::_handleDelete(Client* client, const std::string& realPath,
                                  const LocationConfig* location) {
  (void)client;
  (void)realPath;
  (void)location;
  return 501;
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
//   true if an internal redirection occured, false otherwise.
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
