#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H
#include <unordered_map>//无序键值
#include <unordered_set>//无序集合
#include <string>
#include <regex>//正则
#include <errno.h>
#include <mysql/mysql.h>
#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"

class Http_Request 
{
public:
    enum Parse_State//解析状态
    {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };
    Http_Request()
    {
        Init();
    }
    void Init();
    bool parse(Buffer &buff);
    std::string Get_path() const;//请求路径
    std::string& Get_path();
    std::string Get_method() const;//请求方法
    std::string Get_version() const;//HTTP版本
    std::string Get_Post(const std::string &key) const;//返回POST指定键对应的值
    std::string Get_Post(const char *key) const;

    bool is_keep_alive() const;//心跳检测

private:
    bool Parse_Request_Line (const std::string &line);//解析请求行
    void Parse_Header(const std::string &line);//解析请求头
    void Parse_Body(const std::string &line);//解析请求体
    void Parse_Path();//解析请求路径
    void Parse_Post();//解析POST
    void Parse_From_URL_Encoded();//解析编码
    //用户验证 用户名 密码 登陆状态
    static bool User_Verify(const std::string &name, const std::string &password, bool is_login);
    Parse_State state;
    std::string method, path, version, body;
    std::unordered_map<std::string,std::string>header;//存储 HTTP 请求头信息的哈希表
    std::unordered_map<std::string,std::string>post;//存储 POST 请求体中数据的哈希表
    static const std::unordered_set<std::string> Default_HTML;//存储默认 HTML 文件路径的集合
    static const std::unordered_map<std::string, int> Default_HTML_TAG;//存储默认 HTML 文件路径和标签的映射
    static int Dec_2_Hex(char ch);
};
#endif