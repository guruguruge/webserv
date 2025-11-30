#ifndef MIME_TYPE_HPP
#define MIME_TYPE_HPP

#include <string>

/**
 * MimeType クラス
 * 
 * ファイル拡張子からMIMEタイプを判定する。
 */
class MimeType {
public:
    /**
     * ファイルパスから拡張子を取得し、MIMEタイプを返す
     * @param path ファイルパス
     * @return MIMEタイプ文字列(例: "text/html")
     */
    static std::string getType(const std::string& path);

private:
    /**
     * 拡張子からMIMEタイプを取得
     * @param extension 拡張子(例: ".html")
     * @return MIMEタイプ文字列
     */
    static std::string getTypeFromExtension(const std::string& extension);
};

#endif // MIME_TYPE_HPP
