#include "ErrorPageManager.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>

HttpResponse ErrorPageManager::makeErrorResponse(
    int statusCode,
    const ServerConfig* serverConfig,
    const std::string& message
) {
    HttpResponse response;
    response.setStatusCode(statusCode);
    
    std::string body;
    bool customPageLoaded = false;
    
    // サーバー設定がある場合、カスタムエラーページを探す
    if (serverConfig) {
        std::map<int, std::string>::const_iterator it = serverConfig->errorPages.find(statusCode);
        if (it != serverConfig->errorPages.end()) {
            // カスタムエラーページのパスを取得
            std::string errorPagePath = it->second;
            
            // 相対パスの場合は絶対パスに変換（簡易実装）
            // 本来はdocument rootからの相対パスとして解釈すべき
            if (!errorPagePath.empty() && errorPagePath[0] != '/') {
                errorPagePath = "www/" + errorPagePath;
            } else if (!errorPagePath.empty() && errorPagePath[0] == '/') {
                errorPagePath = "www" + errorPagePath;
            }
            
            // カスタムエラーページを読み込む
            if (readErrorPage(errorPagePath, body)) {
                customPageLoaded = true;
                response.setHeader("Content-Type", "text/html");
            }
        }
    }
    
    // カスタムエラーページが読み込めなかった場合、デフォルトを生成
    if (!customPageLoaded) {
        std::string displayMessage = message.empty() ? response.getReasonPhrase() : message;
        body = generateDefaultErrorPage(statusCode, response.getReasonPhrase(), displayMessage);
        response.setHeader("Content-Type", "text/html");
    }
    
    // Content-Lengthを設定
    std::ostringstream oss;
    oss << body.size();
    response.setHeader("Content-Length", oss.str());
    
    response.setBody(body);
    
    return response;
}

std::string ErrorPageManager::generateDefaultErrorPage(
    int statusCode,
    const std::string& reasonPhrase,
    const std::string& message
) {
    std::ostringstream html;
    
    html << "<!DOCTYPE html>\n";
    html << "<html>\n";
    html << "<head>\n";
    html << "  <meta charset=\"utf-8\">\n";
    html << "  <title>" << statusCode << " " << reasonPhrase << "</title>\n";
    html << "  <style>\n";
    html << "    body {\n";
    html << "      font-family: Arial, sans-serif;\n";
    html << "      margin: 50px;\n";
    html << "      background-color: #f5f5f5;\n";
    html << "    }\n";
    html << "    .error-container {\n";
    html << "      background-color: white;\n";
    html << "      padding: 30px;\n";
    html << "      border-radius: 8px;\n";
    html << "      box-shadow: 0 2px 4px rgba(0,0,0,0.1);\n";
    html << "      max-width: 600px;\n";
    html << "      margin: 0 auto;\n";
    html << "    }\n";
    html << "    h1 {\n";
    html << "      color: #d32f2f;\n";
    html << "      border-bottom: 2px solid #d32f2f;\n";
    html << "      padding-bottom: 10px;\n";
    html << "    }\n";
    html << "    p {\n";
    html << "      color: #666;\n";
    html << "      line-height: 1.6;\n";
    html << "    }\n";
    html << "    .error-code {\n";
    html << "      font-size: 72px;\n";
    html << "      font-weight: bold;\n";
    html << "      color: #d32f2f;\n";
    html << "      margin: 20px 0;\n";
    html << "    }\n";
    html << "  </style>\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "  <div class=\"error-container\">\n";
    html << "    <div class=\"error-code\">" << statusCode << "</div>\n";
    html << "    <h1>" << reasonPhrase << "</h1>\n";
    html << "    <p>" << message << "</p>\n";
    html << "    <hr>\n";
    html << "    <p><small>webserv/1.0</small></p>\n";
    html << "  </div>\n";
    html << "</body>\n";
    html << "</html>\n";
    
    return html.str();
}

bool ErrorPageManager::readErrorPage(const std::string& filePath, std::string& content) {
    // ファイルの存在確認
    struct stat st;
    if (stat(filePath.c_str(), &st) != 0) {
        return false;
    }
    
    // ディレクトリの場合は失敗
    if (S_ISDIR(st.st_mode)) {
        return false;
    }
    
    // ファイルを開く
    std::ifstream file(filePath.c_str(), std::ios::binary);
    if (!file) {
        return false;
    }
    
    // ファイルサイズを取得
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // サイズチェック（大きすぎるファイルは読まない）
    if (fileSize > 1024 * 1024) { // 1MB制限
        return false;
    }
    
    // バッファに読み込み
    content.resize(static_cast<size_t>(fileSize));
    file.read(&content[0], fileSize);
    
    return file.good() || file.eof();
}
