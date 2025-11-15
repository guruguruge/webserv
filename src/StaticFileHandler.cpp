#include "StaticFileHandler.hpp"
#include "MimeType.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <algorithm>

HttpResponse StaticFileHandler::handleGet(
    const HttpRequest& request,
    const ServerConfig& serverConfig,
    const LocationConfig* locationConfig
) {
    (void)serverConfig; // 将来のエラーページ処理で使用
    
    // メソッドチェック
    if (request.method != "GET" && request.method != "HEAD") {
        return makeErrorResponse(405, "Method Not Allowed");
    }
    
    // ロケーション設定がない場合はエラー
    if (!locationConfig) {
        return makeErrorResponse(404, "Not Found");
    }
    
    // allowed_methodsのチェック
    // HEADリクエストはGETが許可されていれば処理可能
    bool methodAllowed = false;
    std::string checkMethod = request.method;
    if (request.method == "HEAD") {
        checkMethod = "GET";
    }
    
    for (size_t i = 0; i < locationConfig->allowedMethods.size(); ++i) {
        if (locationConfig->allowedMethods[i] == checkMethod) {
            methodAllowed = true;
            break;
        }
    }
    
    if (!methodAllowed) {
        return makeErrorResponse(405, "Method Not Allowed");
    }
    
    // 実ファイルパスを構築
    std::string filePath = buildFilePath(locationConfig, request.path);
    
    // ファイル/ディレクトリの存在確認
    if (!fileExists(filePath)) {
        return makeErrorResponse(404, "Not Found");
    }
    
    // ディレクトリの場合
    if (isDirectory(filePath)) {
        // 末尾にスラッシュを追加
        if (filePath[filePath.size() - 1] != '/') {
            filePath += '/';
        }
        
        // インデックスファイルがあればそれを返す
        if (!locationConfig->index.empty()) {
            std::string indexPath = filePath + locationConfig->index;
            if (fileExists(indexPath) && !isDirectory(indexPath)) {
                std::string content;
                if (readFile(indexPath, content)) {
                    HttpResponse response;
                    response.setStatusCode(200);
                    response.setHeader("Content-Type", MimeType::getType(indexPath));
                    
                    std::ostringstream oss;
                    oss << content.size();
                    response.setHeader("Content-Length", oss.str());
                    
                    if (request.method == "GET") {
                        response.setBody(content);
                    }
                    
                    return response;
                } else {
                    return makeErrorResponse(500, "Internal Server Error");
                }
            }
        }
        
        // autoindexが有効ならディレクトリリストを生成
        if (locationConfig->autoindex) {
            std::string listing = generateDirectoryListing(filePath, request.path);
            HttpResponse response;
            response.setStatusCode(200);
            response.setHeader("Content-Type", "text/html");
            
            std::ostringstream oss;
            oss << listing.size();
            response.setHeader("Content-Length", oss.str());
            
            if (request.method == "GET") {
                response.setBody(listing);
            }
            
            return response;
        }
        
        // autoindexが無効なら403
        return makeErrorResponse(403, "Forbidden");
    }
    
    // 通常のファイル
    std::string content;
    if (!readFile(filePath, content)) {
        return makeErrorResponse(500, "Internal Server Error");
    }
    
    HttpResponse response;
    response.setStatusCode(200);
    response.setHeader("Content-Type", MimeType::getType(filePath));
    
    std::ostringstream oss;
    oss << content.size();
    response.setHeader("Content-Length", oss.str());
    
    if (request.method == "GET") {
        response.setBody(content);
    }
    
    return response;
}

std::string StaticFileHandler::buildFilePath(
    const LocationConfig* locationConfig,
    const std::string& requestPath
) {
    if (!locationConfig) {
        return requestPath;
    }
    
    // ロケーションパスを除去
    std::string relativePath = requestPath;
    if (requestPath.find(locationConfig->path) == 0) {
        relativePath = requestPath.substr(locationConfig->path.size());
    }
    
    // 先頭のスラッシュを除去
    if (!relativePath.empty() && relativePath[0] == '/') {
        relativePath = relativePath.substr(1);
    }
    
    // root + relativePath
    std::string filePath = locationConfig->root;
    if (!filePath.empty() && filePath[filePath.size() - 1] != '/') {
        filePath += '/';
    }
    filePath += relativePath;
    
    return filePath;
}

bool StaticFileHandler::isDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

bool StaticFileHandler::fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool StaticFileHandler::readFile(const std::string& path, std::string& content) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file) {
        return false;
    }
    
    // ファイルサイズを取得
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // サイズチェック（大きすぎるファイルは読まない）
    if (fileSize > 10 * 1024 * 1024) { // 10MB制限
        return false;
    }
    
    // バッファに読み込み
    content.resize(static_cast<size_t>(fileSize));
    file.read(&content[0], fileSize);
    
    return file.good() || file.eof();
}

std::string StaticFileHandler::generateDirectoryListing(
    const std::string& dirPath,
    const std::string& requestPath
) {
    std::vector<std::string> entries = listDirectory(dirPath);
    
    std::ostringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html>\n";
    html << "<head>\n";
    html << "  <meta charset=\"utf-8\">\n";
    html << "  <title>Index of " << requestPath << "</title>\n";
    html << "  <style>\n";
    html << "    body { font-family: Arial, sans-serif; margin: 40px; }\n";
    html << "    h1 { border-bottom: 1px solid #ccc; padding-bottom: 10px; }\n";
    html << "    ul { list-style: none; padding: 0; }\n";
    html << "    li { padding: 5px 0; }\n";
    html << "    a { text-decoration: none; color: #0066cc; }\n";
    html << "    a:hover { text-decoration: underline; }\n";
    html << "    .dir { font-weight: bold; }\n";
    html << "  </style>\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "  <h1>Index of " << requestPath << "</h1>\n";
    html << "  <ul>\n";
    
    // 親ディレクトリへのリンク
    if (requestPath != "/") {
        html << "    <li><a href=\"../\" class=\"dir\">[Parent Directory]</a></li>\n";
    }
    
    // エントリをソート
    std::sort(entries.begin(), entries.end());
    
    // 各エントリへのリンク
    for (size_t i = 0; i < entries.size(); ++i) {
        std::string fullPath = dirPath;
        if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/') {
            fullPath += '/';
        }
        fullPath += entries[i];
        
        bool isDir = isDirectory(fullPath);
        std::string displayName = entries[i];
        
        if (isDir) {
            displayName += '/';
        }
        
        html << "    <li><a href=\"" << entries[i];
        if (isDir) {
            html << "/";
        }
        html << "\"";
        if (isDir) {
            html << " class=\"dir\"";
        }
        html << ">" << displayName << "</a></li>\n";
    }
    
    html << "  </ul>\n";
    html << "</body>\n";
    html << "</html>\n";
    
    return html.str();
}

std::vector<std::string> StaticFileHandler::listDirectory(const std::string& dirPath) {
    std::vector<std::string> entries;
    
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) {
        return entries;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        
        // "." と ".." をスキップ
        if (name == "." || name == "..") {
            continue;
        }
        
        entries.push_back(name);
    }
    
    closedir(dir);
    return entries;
}

HttpResponse StaticFileHandler::makeErrorResponse(int statusCode, const std::string& message) {
    HttpResponse response;
    response.setStatusCode(statusCode);
    
    std::ostringstream body;
    body << "<!DOCTYPE html>\n";
    body << "<html>\n";
    body << "<head><title>" << statusCode << " " << response.getReasonPhrase() << "</title></head>\n";
    body << "<body>\n";
    body << "<h1>" << statusCode << " " << response.getReasonPhrase() << "</h1>\n";
    body << "<p>" << message << "</p>\n";
    body << "</body>\n";
    body << "</html>\n";
    
    response.setHeader("Content-Type", "text/html");
    
    std::ostringstream oss;
    oss << body.str().size();
    response.setHeader("Content-Length", oss.str());
    
    response.setBody(body.str());
    
    return response;
}
