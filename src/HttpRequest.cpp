#include "../inc/Http.hpp"

// =============================================================================
// Default Constructor
// =============================================================================
HttpRequest::HttpRequest()
    : _config(NULL), _location(NULL)
{
    clear();
}

// =============================================================================
// Destructor
// =============================================================================
HttpRequest::~HttpRequest()
{
    // 動的リソースは持っていないので特に何もしない
}

// =============================================================================
// clear - 全メンバを初期状態にリセット（Keep-Alive用）
// =============================================================================
void HttpRequest::clear()
{
    _buffer.clear();
    _parseState = REQ_REQUEST_LINE;
    _error = ERR_NONE;

    _method = UNKNOWN_METHOD;
    _path.clear();
    _query.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
}
