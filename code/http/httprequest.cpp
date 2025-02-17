#include "httprequest.h"
using namespace std;
const unordered_set<string> Http_Request::Default_HTML
{
    "/index","/register","/login","/welcome","/video","/picture",
};

const unordered_map<string, int> Http_Request::Default_HTML_TAG
{
    {"/register.html",0},{"/login.html",1},
};

void Http_Request::Init()
{
    method="";
    path="";
    version="";
    body="";
    state=REQUEST_LINE;
    header.clear();
    post.clear();
}

bool Http_Request::is_keep_alive() const
{
    //如果存在连接字段且对应的值为keep-alive 同时version对应的值为1.1则保持连接
    if(header.count("Connection")==1)//无序键值对有且仅有一对 相当于检查是否有连接键值对
        return header.find("Connection")->second == "keep-alive" && version =="1.1";
    return false;
}

bool Http_Request::parse(Buffer &buff)
{
    const char CRLF[]="\r\n";//回车换行
    if(buff.Readable_Bytes()<=0)
        return false;
    while(buff.Readable_Bytes()&&state!=FINISH)
    {
        //未读取数据先去除"\r\n" 返回有效数据行末指针
        //也就是获取一行的尾指针
        //处理行时根据当前解析状态执行对应解析操作
        //随后读取回车换行 将读指针移动到下一行行头
        //当行末指针指向未读取数据末尾时表示读取末行
        //此时数据读取完毕
        const char* line_end=search(buff.Peek(),buff.Begin_Write_Const(),CRLF,CRLF+2);
        std::string line(buff.Peek(),line_end);//将该行从字符串转string
        switch(state)
        {
            case REQUEST_LINE:
                if(!Parse_Request_Line(line))
                    return false;
                Parse_Path();
                break;
            case HEADERS:
                Parse_Header(line);
                //所有头部行结束后以一个单独的 \r\n 表示头部的结束
                //如果可读字节小于等于2 buff剩余可读字节\r\n 表示头部的结束
                if(buff.Readable_Bytes()<=2)
                    state=FINISH;
                break;
            case BODY:
                Parse_Body(line);
                break;
            default:
                break;
        }
        if(line_end==buff.Begin_Write())
            break;
        buff.Retrieve_Until(line_end+2);//跳过\r\n 将读指针移动到下一行行头
    }
    LOG_DEBUG("[%s],[%s],[%s]",method.c_str(),path.c_str(),version.c_str());
    return true;
}

void Http_Request::Parse_Path()
{
    //如果path是根路径则设置为 "/index.html"
    //否则检查path是否在默认的HTML文件集合中
    //如果在则添加 ".html" 后缀
    if(path=="/")
        path="/index.html";
    else
    {
        for(auto &item: Default_HTML)
        {
            if(item==path)
            {
                path+=".html";
                break;
            }
        }
    }
}

bool Http_Request::Parse_Request_Line(const string &line)
{
    regex reg("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    //^ 匹配字符串的开始
    //([^ ]*) 匹配一段没有空格的字符 表示 HTTP 方法 sub_Match[1]
    //[ ] 匹配一个空格字符
    //([^ ]*) 匹配一段没有空格的字符 表示请求路径 sub_Match[2]
    //[ ] 匹配一个空格字符
    //HTTP/([^ ]*)$ 匹配以 HTTP/ 开头 后面跟随一段没有空格的字符
    //表示 HTTP 版本 sub_Match[3]
    smatch sub_Match;//存储匹配结果
    if(regex_match(line,sub_Match,reg))
    {
        method=sub_Match[1];
        path=sub_Match[2];
        version=sub_Match[3];
        state=HEADERS;//解析请求行完毕
        return true;
    }
    LOG_ERROR("Request Line Error");
    return false;
}

void Http_Request::Parse_Header(const string &line)
{
    //^ 匹配字符串的开始
    //([^:]*) 匹配一段没有冒号的字符 表示请求头的名称 subMatch[1]
    //: 匹配一个冒号字符
    //? 匹配 0 个或 1 个空格字符
    //(.*)$ 匹配任意数量的字符 表示请求头的值 subMatch[2]
    regex reg("^([^:]*): ?(.*)$");
    smatch sub_Match;
    if(regex_match(line,sub_Match,reg))
        header[sub_Match[1]]=sub_Match[2];//将请求头的名称和值存储在header哈希表中
    else
        state=BODY;
}

void Http_Request::Parse_Body(const string &line)
{
    body=line;
    Parse_Post();
    state=FINISH;
    LOG_DEBUG("Body:%s, len:%d",line.c_str(),line.size());
}

void Http_Request::Parse_Post()
{
    //请求方法为POST且请求体是URL编码的表单数据
    if(method=="POST"&&header["Content-Type"]=="application/x-www-form-urlencoded")
    {
        Parse_From_URL_Encoded();//解析URL编码的表单数据并将结果存储在post哈希表中
        if(Default_HTML_TAG.count(path))
        {
            int tag=Default_HTML_TAG.find(path)->second;
            LOG_DEBUG("Tag:%d",tag);
            if(tag==0||tag==1)
            {
                bool is_login=(tag==1);//登录或者注册
                //验证成功 登录或者注册
                if(User_Verify(post["username"],post["password"],is_login))
                    path="/welcome.html";
                else//验证失败
                    path="/error.html";
            }
        }
    }
}

void Http_Request::Parse_From_URL_Encoded()
{
    if(body.size()==0)
        return;
    string key,value;
    int num=0;
    int n=body.size();
    int i=0,j=0;
    for(;i<n;i++)//逐字符遍历body 根据字符的类型进行不同的处理
    {
        char ch=body[i];
        switch (ch) 
        {
            //=表示键的结束和值的开始
            //提取字符串作为键 并更新 j 为下一个字符的位置
            case '=':
                key = body.substr(j, i - j);
                j = i + 1;
                break;
            //将+转换为空格
            case '+':
                body[i] = ' ';
                break;
            //% 表示 URL 编码的特殊字符
            //将十六进制表示的字符转换为十进制 更新请求体中的字符并跳过两个字符
            case '%':
                num = Dec_2_Hex(body[i + 1]) * 16 + Dec_2_Hex(body[i + 2]);
                body[i + 2] = num % 10 + '0';
                body[i + 1] = num / 10 + '0';
                i += 2;
                break;
            //&表示一个键值对的结束
            //每当遍历到&时就存储一对键值对
            //提取字符串作为值 并将键值对存储在post哈希表中 更新 j 为下一个字符的位置
            case '&':
                value = body.substr(j, i - j);
                j = i + 1;
                post[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default:
                break;
        }

    }
    //在解析URL编码的表单数据时多个键值对之间用&分隔
    //由于最后一个键值对末尾不会被&分隔
    //需要额外的处理来确保该键值对能够正确解析并存储
    assert(j<=i);
    if(post.count(key)==0&&j<i)
    {
        value=body.substr(j,i-j);
        post[key]=value;
    }
}

int Http_Request::Dec_2_Hex(char ch)
{
    if(ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if(ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return ch;
}

bool Http_Request::User_Verify(const string &name,const string &password, bool is_login)
{
    if(name==""||password=="")
        return false;
    LOG_INFO("Verify name:%s password:%s",name.c_str(),password.c_str());
    MYSQL *sql;
    Sql_Conn_RAII(&sql, Sql_Conn_Pool::Instance());
    assert(sql);
    bool flag=false;//验证结果
    //unsigned int j=0;
    char order[256]={0};//查询语句
    //MYSQL_FIELD *fields=nullptr;
    MYSQL_RES *res=nullptr;
    if(!is_login)
        flag=true;
    snprintf(order,256,"SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s",order);
    //如果查询成功 返回0 如果是SELECT 结果集在MYSQL对象sql中
    //如果出现错误 返回非0值。
    if(mysql_query(sql,order))//如果查询失败 释放查询结果
    {
        mysql_free_result(res);
        return false;
    }
    res=mysql_store_result(sql);
    //j=mysql_num_fields(res);//列数
    //fields=mysql_fetch_fields(res);//字段

    while(MYSQL_ROW row=mysql_fetch_row(res))
    //遍历user表中指定username的字段
    //如果登录 密码正确通过验证 返回true
    //如果注册 用户名不同通过验证 继续登记操作
    {
        LOG_DEBUG("MYSQL ROW: %s %s",row[0],row[1]);
        string password_query(row[1]);
        if(is_login)
        {
            if(password==password_query)
                flag=true;
            else
            {
                flag=false;
                LOG_INFO("password error");
            }
        }
        else
        {
            flag=false;
            LOG_INFO("User used");
        }
    }
    mysql_free_result(res);

    if(!is_login&&flag==true)
    {
        LOG_DEBUG("Register");
        bzero(order,256);
        snprintf(order,256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), password.c_str());
        LOG_DEBUG("%s", order);
        if(mysql_query(sql,order))
        {
            LOG_DEBUG("Insert error");
            flag=false;
        }
        flag=true;
    }
    //Sql_Conn_Pool::Instance()->Free_Conn(sql);
    LOG_DEBUG("User Vertify Success");
    return flag;
}

std::string Http_Request::Get_path() const
{
    return path;
}

std::string& Http_Request::Get_path()
{
    return path;
}

std::string Http_Request::Get_method() const
{
    return method;
}

std::string Http_Request::Get_version() const
{
    return version;
}

std::string Http_Request::Get_Post(const std::string &key) const
{
    assert(key!="");
    if(post.count(key)==1)
        return post.find(key)->second;
    return "";
}

std::string Http_Request::Get_Post(const char *key) const
{
    assert(key!=nullptr);
    if(post.count(key)==1)
        return post.find(key)->second;
    return "";
}