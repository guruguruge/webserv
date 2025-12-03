#include "UploadHandler.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include "ErrorPageManager.hpp"

HttpResponse UploadHandler::handlePost(const HttpRequest& req,
                                       const ServerConfig& server,
                                       const LocationConfig* location) {
  // location が指定されていない場合はエラー
  if (!location) {
    return ErrorPageManager::makeErrorResponse(404, &server,
                                               "Location not found");
  }

  // upload_path が設定されていない場合はエラー
  if (location->uploadPath.empty()) {
    return ErrorPageManager::makeErrorResponse(403, &server,
                                               "Upload not allowed");
  }

  // client_max_body_size のチェック
  if (server.clientMaxBodySize > 0 &&
      req.body.size() > server.clientMaxBodySize) {
    return ErrorPageManager::makeErrorResponse(413, &server,
                                               "Request entity too large");
  }

  // Content-Type ヘッダを取得
  std::map<std::string, std::string>::const_iterator it =
      req.headers.find("content-type");
  std::string contentType;
  if (it != req.headers.end()) {
    contentType = it->second;
  }

  // multipart/form-data かどうかをチェック
  if (contentType.find("multipart/form-data") != std::string::npos) {
    return handleMultipart(req, location);
  } else {
    return handleRaw(req, location);
  }
}

HttpResponse UploadHandler::handleMultipart(const HttpRequest& req,
                                            const LocationConfig* location) {
  // Content-Type から boundary を抽出
  std::map<std::string, std::string>::const_iterator it =
      req.headers.find("content-type");
  if (it == req.headers.end()) {
    HttpResponse response;
    response.setStatusCode(400);
    response.setReasonPhrase("Bad Request");
    response.setBody("Content-Type header missing");
    return response;
  }

  std::string boundary = extractBoundary(it->second);
  if (boundary.empty()) {
    HttpResponse response;
    response.setStatusCode(400);
    response.setReasonPhrase("Bad Request");
    response.setBody("Boundary not found in Content-Type");
    return response;
  }

  // boundary を -- で囲む
  std::string delimiter = "--" + boundary;
  std::string endDelimiter = delimiter + "--";

  // body を std::string に変換
  std::string body(req.body.begin(), req.body.end());

  size_t pos = 0;
  int filesUploaded = 0;
  std::string uploadedFiles;

  while (pos < body.size()) {
    // delimiter を探す
    size_t delimiterPos = body.find(delimiter, pos);
    if (delimiterPos == std::string::npos) {
      break;
    }

    // 次の delimiter を探す
    size_t nextDelimiterPos =
        body.find(delimiter, delimiterPos + delimiter.size());
    if (nextDelimiterPos == std::string::npos) {
      break;
    }

    // このパートを抽出
    std::string part =
        body.substr(delimiterPos + delimiter.size(),
                    nextDelimiterPos - (delimiterPos + delimiter.size()));

    // ヘッダとボディを分割
    size_t headerEnd = part.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
      headerEnd = part.find("\n\n");
      if (headerEnd == std::string::npos) {
        pos = nextDelimiterPos;
        continue;
      }
      headerEnd += 2;
    } else {
      headerEnd += 4;
    }

    std::string headers = part.substr(0, headerEnd);
    std::string content = part.substr(headerEnd);

    // 末尾の改行を削除
    if (content.size() >= 2 && content.substr(content.size() - 2) == "\r\n") {
      content = content.substr(0, content.size() - 2);
    } else if (content.size() >= 1 && content[content.size() - 1] == '\n') {
      content = content.substr(0, content.size() - 1);
    }

    // Content-Disposition からファイル名を抽出
    std::string filename;
    size_t cdPos = headers.find("Content-Disposition:");
    if (cdPos != std::string::npos) {
      size_t lineEnd = headers.find("\n", cdPos);
      std::string cdLine = headers.substr(cdPos, lineEnd - cdPos);
      filename = extractFilename(cdLine);
    }

    // ファイル名が見つからない場合は生成
    if (filename.empty()) {
      filename = generateUniqueFilename();
    }

    // ファイルを保存
    std::string filepath = location->uploadPath + "/" + filename;
    if (saveFile(filepath, content)) {
      filesUploaded++;
      if (!uploadedFiles.empty()) {
        uploadedFiles += ", ";
      }
      uploadedFiles += filename;
    }

    pos = nextDelimiterPos;
  }

  // レスポンスを作成
  HttpResponse response;
  if (filesUploaded > 0) {
    response.setStatusCode(201);
    response.setReasonPhrase("Created");
    std::ostringstream oss;
    oss << "<!DOCTYPE html>\n"
        << "<html>\n"
        << "<head><title>Upload Success</title></head>\n"
        << "<body>\n"
        << "<h1>Upload Successful</h1>\n"
        << "<p>" << filesUploaded << " file(s) uploaded: " << uploadedFiles
        << "</p>\n"
        << "</body>\n"
        << "</html>\n";
    response.setBody(oss.str());
    response.setHeader("Content-Type", "text/html");
  } else {
    response.setStatusCode(400);
    response.setReasonPhrase("Bad Request");
    response.setBody("No files found in multipart data");
  }

  return response;
}

HttpResponse UploadHandler::handleRaw(const HttpRequest& req,
                                      const LocationConfig* location) {
  // ファイル名を生成
  std::string filename = generateUniqueFilename();
  std::string filepath = location->uploadPath + "/" + filename;

  // body を std::string に変換
  std::string bodyStr(req.body.begin(), req.body.end());

  // ファイルを保存
  if (!saveFile(filepath, bodyStr)) {
    HttpResponse response;
    response.setStatusCode(500);
    response.setReasonPhrase("Internal Server Error");
    response.setBody("Failed to save file");
    return response;
  }

  // レスポンスを作成
  HttpResponse response;
  response.setStatusCode(201);
  response.setReasonPhrase("Created");
  std::ostringstream oss;
  oss << "<!DOCTYPE html>\n"
      << "<html>\n"
      << "<head><title>Upload Success</title></head>\n"
      << "<body>\n"
      << "<h1>Upload Successful</h1>\n"
      << "<p>File saved as: " << filename << "</p>\n"
      << "</body>\n"
      << "</html>\n";
  response.setBody(oss.str());
  response.setHeader("Content-Type", "text/html");

  return response;
}

std::string UploadHandler::extractBoundary(const std::string& contentType) {
  size_t pos = contentType.find("boundary=");
  if (pos == std::string::npos) {
    return "";
  }

  pos += 9;  // "boundary=" の長さ
  size_t end = contentType.find(";", pos);
  if (end == std::string::npos) {
    end = contentType.size();
  }

  std::string boundary = contentType.substr(pos, end - pos);

  // 前後の空白を削除
  size_t start = 0;
  while (start < boundary.size() &&
         (boundary[start] == ' ' || boundary[start] == '\t')) {
    start++;
  }
  size_t finish = boundary.size();
  while (finish > start &&
         (boundary[finish - 1] == ' ' || boundary[finish - 1] == '\t')) {
    finish--;
  }

  return boundary.substr(start, finish - start);
}

std::string UploadHandler::extractFilename(
    const std::string& contentDisposition) {
  size_t pos = contentDisposition.find("filename=");
  if (pos == std::string::npos) {
    return "";
  }

  pos += 9;  // "filename=" の長さ

  // 引用符で囲まれているかチェック
  if (pos < contentDisposition.size() && contentDisposition[pos] == '"') {
    pos++;
    size_t end = contentDisposition.find('"', pos);
    if (end != std::string::npos) {
      return contentDisposition.substr(pos, end - pos);
    }
  }

  // 引用符なしの場合
  size_t end = contentDisposition.find_first_of(";\r\n", pos);
  if (end == std::string::npos) {
    end = contentDisposition.size();
  }

  return contentDisposition.substr(pos, end - pos);
}

std::string UploadHandler::generateUniqueFilename() {
  std::ostringstream oss;
  oss << "upload_" << time(NULL) << "_" << (rand() % 10000) << ".dat";
  return oss.str();
}

bool UploadHandler::saveFile(const std::string& path,
                             const std::string& content) {
  // ディレクトリが存在するか確認
  size_t lastSlash = path.find_last_of('/');
  if (lastSlash != std::string::npos) {
    std::string dir = path.substr(0, lastSlash);
    struct stat st;
    if (stat(dir.c_str(), &st) != 0) {
      // ディレクトリが存在しない場合は作成
      if (mkdir(dir.c_str(), 0755) != 0) {
        return false;
      }
    }
  }

  // ファイルを書き込み
  std::ofstream ofs(path.c_str(), std::ios::binary);
  if (!ofs) {
    return false;
  }

  ofs.write(content.c_str(), content.size());
  ofs.close();

  return ofs.good();
}
