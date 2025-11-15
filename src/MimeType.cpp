#include "MimeType.hpp"
#include <algorithm>

std::string MimeType::getType(const std::string& path) {
    // ファイルパスから拡張子を取得
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "application/octet-stream"; // デフォルト
    }
    
    std::string extension = path.substr(dotPos);
    
    // 拡張子を小文字化
    for (size_t i = 0; i < extension.size(); ++i) {
        extension[i] = std::tolower(static_cast<unsigned char>(extension[i]));
    }
    
    return getTypeFromExtension(extension);
}

std::string MimeType::getTypeFromExtension(const std::string& ext) {
    // テキスト系
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "text/javascript";
    if (ext == ".txt") return "text/plain";
    if (ext == ".xml") return "text/xml";
    if (ext == ".csv") return "text/csv";
    
    // 画像系
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".bmp") return "image/bmp";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    
    // アプリケーション系
    if (ext == ".json") return "application/json";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".zip") return "application/zip";
    if (ext == ".tar") return "application/x-tar";
    if (ext == ".gz") return "application/gzip";
    
    // 音声・動画系
    if (ext == ".mp3") return "audio/mpeg";
    if (ext == ".wav") return "audio/wav";
    if (ext == ".mp4") return "video/mp4";
    if (ext == ".avi") return "video/x-msvideo";
    
    // その他
    return "application/octet-stream";
}
