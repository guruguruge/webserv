#ifndef ERROR_PAGE_MANAGER_HPP
#define ERROR_PAGE_MANAGER_HPP

#include <string>
#include "Config.hpp"
#include "HttpResponse.hpp"

/**
 * ErrorPageManager クラス
 * 
 * エラーレスポンスの生成を統一的に管理する。
 * - カスタムエラーページの読み込み
 * - デフォルトエラーページの生成
 */
class ErrorPageManager {
 public:
  /**
     * エラーレスポンスを生成
     * 
     * @param statusCode HTTPステータスコード
     * @param serverConfig サーバー設定（error_page設定を参照）
     * @param message エラーメッセージ（省略可能）
     * @return HTTPレスポンス
     */
  static HttpResponse makeErrorResponse(int statusCode,
                                        const ServerConfig* serverConfig = NULL,
                                        const std::string& message = "");

 private:
  /**
     * デフォルトエラーページのHTMLを生成
     */
  static std::string generateDefaultErrorPage(int statusCode,
                                              const std::string& reasonPhrase,
                                              const std::string& message);

  /**
     * カスタムエラーページを読み込む
     * @param filePath エラーページのファイルパス
     * @param content 読み込んだ内容を格納する変数
     * @return 成功したらtrue
     */
  static bool readErrorPage(const std::string& filePath, std::string& content);
};

#endif  // ERROR_PAGE_MANAGER_HPP
