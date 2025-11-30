#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include "ClientConnection.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Config.hpp"
#include <string>
#include <vector>

class CgiHandler {
public:
    /**
     * CGIプロセスを開始
     * - fork()して子プロセスでexecve()
     * - pipeでstdin/stdoutを接続
     * - 環境変数を構築
     * - Pollerに stdout fd を登録
     * @return true: 成功, false: 失敗(スクリプトが存在しない等)
     */
    static bool startCgi(
        ClientConnection& client,
        const HttpRequest& req,
        const ServerConfig& server,
        const LocationConfig& location
    );

    /**
     * CGI stdoutが読み取り可能になったときの処理
     * - stdout から読み取り
     * - EOF になったらHTTPレスポンスに変換
     * @return true: CGI完了, false: 継続中
     */
    static bool onCgiStdoutReadable(ClientConnection& client);

private:
    /**
     * CGI用の環境変数を構築
     */
    static char** buildEnvp(
        const HttpRequest& req,
        const ServerConfig& server,
        const LocationConfig& location,
        const std::string& scriptPath
    );

    /**
     * CGI用のargvを構築
     */
    static char** buildArgv(
        const LocationConfig& location,
        const std::string& scriptPath
    );

    /**
     * envp/argvを解放
     */
    static void freeEnvp(char** envp);
    static void freeArgv(char** argv);

    /**
     * CGI出力をHTTPレスポンスに変換
     */
    static HttpResponse parseCgiOutput(const std::string& cgiOutput);

    /**
     * スクリプトファイルのパスを解決
     */
    static std::string resolveScriptPath(
        const HttpRequest& req,
        const LocationConfig& location
    );
};

#endif // CGI_HANDLER_HPP
