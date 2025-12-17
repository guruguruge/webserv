#include "RequestHandler.hpp"

RequestHandler::RequestHandler(const MainConfig& config) : _config(config) {}

RequestHandler::~RequestHandler() {}

void RequestHandler::handle(Client* client) {
  if (!client)
    return;

  const HttpRequest& req = client->req;

  const ServerConfig* matchedServer = _findServerConfig(client);
  if (!matchedServer) {
    _handleError(client, 500);
    return;
  }

  const LocationConfig* matchedLocation =
      _findLocationConfig(client->req, *matchedServer);

  std::string realPath =
      _resolvePath(client->req.getPath(), *matchedServer, matchedLocation);

  switch (req.getMethod()) {
    case GET:
      _handleGet(client, realPath, matchedLocation);
      break;
    case POST:
      _handlePost(client, realPath, matchedLocation);
      break;
    case DELETE:
      _handleDelete(client, realPath, matchedLocation);
      break;
    default:
      _handleError(client, 405);
      break;
  }
}

const ServerConfig* RequestHandler::_findServerConfig(const Client* client) {
  int port = client->getListenPort();
  std::string hostHeader = client->req.getHeader("Host");
  std::string hostName = hostHeader;

  size_t colonPos = hostName.find(':');
  if (colonPos != std::string::npos) {
    hostName = hostName.substr(0, colonPos);
  }
  const ServerConfig* defaultServer = NULL;

  for (std::vector<ServerConfig>::const_iterator it = _config.servers.begin();
       it != _config.servers.end(); ++it) {
    if (it->listen_port != port) {
      continue;
    }
    if (defaultServer == NULL) {
      defaultServer = &(*it);
    }
    for (std::vector<std::string>::const_iterator nameIt =
             it->server_names.begin();
         nameIt != it->server_names.end(); ++nameIt) {
      if (*nameIt == hostName) {
        return &(*it);
      }
    }
  }
  return defaultServer;
}

// 正規表現エンジンを作るのがだるすぎるので、一旦は簡易実装
const LocationConfig* RequestHandler::_findLocationConfig(
    const HttpRequest& req, const ServerConfig& serverConfig) {

  const LocationConfig* bestPrefixMatch = NULL;
  size_t maxPrefixLength = 0;
  std::string uri = req.getPath();
  for (std::vector<LocationConfig>::const_iterator it =
           serverConfig.locations.begin();
       it != serverConfig.locations.end(); ++it) {
    if (uri.compare(0, it->path.length(), it->path) == 0) {
      if (uri.length() > it->path.length() &&
          it->path[it->path.length() - 1] != '/' &&
          uri[it->path.length()] != '/')
        continue;
      if (it->path.length() > maxPrefixLength) {
        maxPrefixLength = it->path.length();
        bestPrefixMatch = &(*it);
      }
    }
  }
  return bestPrefixMatch;
  //   for (std::vector<LocationConfig>::const_iterator it =
  //            serverConfig.locations.begin();
  //        it != serverConfig.locations.end(); ++it) {
  //     if (it->modifier == "=") {
  //       if (uri == it->path) {
  //         return &(*it);
  //       }
  //       continue;
  //     }
  //     if (it->modifier == "^~" || it->modifier.empty()) {
  //       if (uri.compare(0, it->path.length(), it->path) == 0) {
  //         if (it->path.length() > maxPrefixLength) {
  //           maxPrefixLength = it->path.length();
  //           bestPrefixMatch = &(*it);
  //         }
  //       }
  //     }
  //     if (bestPrefixMatch && bestPrefixMatch->modifier == "^~") {
  //       return bestPrefixMatch;
  //     }
  //     for (std::vector<LocationConfig>::const_iterator it =
  //              serverConfig.locations.begin();
  //          it != serverConfig.locations.end(); ++it) {
  //       if (it->modifier == "~" || it->modifier == "~*") {
  //         if (_isRegexMatch(uri, it->path, (it->modifier == "~*"))) {
  //           return &(*it);
  //         }
  //       }
  //     }
  //     return bestPrefixMatch;
  //   }
}

std::string RequestHandler::_resolvePath(const std::string& uri,
                                         const ServerConfig& serverConfig,
                                         const LocationConfig* location) {
  if (!location) {
    return serverConfig.root + uri;
  }
  std::string path = uri;
  if (!location->alias.empty()) {
    path.replace(0, location->path.length(), location->alias);
  } else {
    std::string root =
        location->root.empty() ? serverConfig.root : location->root;
    path = location->root + uri;
  }
  return path;
}

// 以下、スケルトン実装
void RequestHandler::_handleGet(Client* client, const std::string& realPath,
                                const LocationConfig* location) {
  (void)location;
  (void)realPath;

  // 仮レスポンス
  HttpResponse res;
  res.setStatusCode(200);
  res.setBody("Hello from GET! Path resolved to: " + realPath);
  client->res = res;
}

void RequestHandler::_handlePost(Client* client, const std::string& realPath,
                                 const LocationConfig* location) {
  (void)realPath;
  (void)location;
  _handleError(client, 501);
}

void RequestHandler::_handleDelete(Client* client, const std::string& realPath,
                                   const LocationConfig* location) {
  (void)realPath;
  (void)location;
  _handleError(client, 501);
}

void RequestHandler::_handleError(Client* client, int statusCode) {
  HttpResponse res;
  res.setStatusCode(statusCode);
  client->res = res;
}
