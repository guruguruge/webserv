#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "Config.hpp"
#include "HttpRequest.hpp"

/**
 * Router クラス
 * 
 * HTTPリクエストから適切なServerConfigとLocationConfigを選択する。
 */
class Router {
public:
    /**
     * リクエストに対応するServerConfigを検索
     * 
     * @param config 全体の設定
     * @param request HTTPリクエスト
     * @return 対応するServerConfig（見つからない場合は最初のサーバー）
     */
    static const ServerConfig* findServer(const Config& config, const HttpRequest& request);
    
    /**
     * パスに対応するLocationConfigを検索
     * 
     * @param serverConfig サーバー設定
     * @param path リクエストパス
     * @return 対応するLocationConfig（見つからない場合はNULL）
     */
    static const LocationConfig* findLocation(const ServerConfig& serverConfig, const std::string& path);

private:
    /**
     * ポート番号がServerConfigのlistenリストに含まれているかチェック
     */
    static bool isPortMatching(const ServerConfig& serverConfig, int port);
};

#endif // ROUTER_HPP
