#include "httpresponse.h"
using namespace std;

const unordered_map<string, string> Http_Response::SUFFIX_TYPE = 
{
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const unordered_map<int, string> Http_Response::CODE_STATUS = 
{
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const unordered_map<int, string> Http_Response::CODE_PATH = 
{
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

Http_Response::Http_Response()
{
    rp_code=-1;
    rp_path="";
    rp_src_Dir="";
    rp_is_Keep_Alive=false;
    mm_File=nullptr;
    mm_File_Stat={0};
}

Http_Response::~Http_Response()
{
    Un_map_File();
}

void Http_Response::Un_map_File()
{
    if(mm_File)
    {
        munmap(mm_File,mm_File_Stat.st_size);
        mm_File=nullptr;
    }  
}

void Http_Response::Init(const string &src_Dir, string &path,bool is_Keep_Alive,int code)
{
    assert(src_Dir!="");
    if(mm_File)
        Un_map_File();
    rp_code=code;
    rp_is_Keep_Alive=is_Keep_Alive;
    rp_path=path;
    rp_src_Dir=src_Dir;
    mm_File=nullptr;
    mm_File_Stat={0};
}

void Http_Response::Make_Response(Buffer &buff)
{
    //获取文件的状态信息并将其存储在mm_File_Stat结构体中
    //如果返回值小于0表示文件不存在或路径错误
    //检查文件类型是否为目录
    //如果文件不存在或者是一个目录 404 not found
    if(stat((rp_src_Dir + rp_path).data(), &mm_File_Stat) < 0 || S_ISDIR(mm_File_Stat.st_mode))
        rp_code = 404;
    //检查文件是否对其他用户具有读取权限
    //如果文件对其他用户不可读 403 Forbidden
    else if(!(mm_File_Stat.st_mode & S_IROTH))
        rp_code = 403;
    //如果code仍然是默认值 200 OK
    else if(rp_code == -1)
        rp_code = 200; 
    Error_Html();
    Add_State_Line(buff);
    Add_Header(buff);
    Add_Content(buff);
}

char* Http_Response::File()
{
    return mm_File;
}

size_t Http_Response::File_Len() const
{
    return mm_File_Stat.st_size;
}

void Http_Response::Error_Html()
{
    if(CODE_PATH.count(rp_code)==1)
    {
        rp_path=CODE_PATH.find(rp_code)->second;
        //获取错误页面文件的状态信息并将其存储在mm_File_Stat结构体中
        //后续处理可以基于这些信息来生成响应
        stat((rp_src_Dir+rp_path).data(),&mm_File_Stat);
    }
}

void Http_Response::Add_State_Line(Buffer &buff)
{
    string status;
    if(CODE_STATUS.count(rp_code)==1)
        status=CODE_STATUS.find(rp_code)->second;
    else
    //请求错误 400 Bad Request
    {
        rp_code=400;
        status=CODE_STATUS.find(rp_code)->second;
    }   
    buff.Append("HTTP/1.1"+to_string(rp_code)+" "+status+"\r\n");  
}

void Http_Response::Add_Header(Buffer &buff)
{
    buff.Append("Connection: ");
    if(rp_is_Keep_Alive)
    {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    }
    else
        buff.Append("close\r\n");
    buff.Append("Content-type: "+Get_File_Type()+"\r\n");
}

void Http_Response::Add_Content(Buffer &buff)
{
    int src_Fd=open((rp_src_Dir+rp_path).data(),O_RDONLY);
    if(src_Fd<0)
    {
        Error_Content(buff,"File Not Found");
        return;
    }
    LOG_DEBUG("file path %s",(rp_src_Dir+rp_path).data());
    //mmap 函数用于将文件映射到内存以提高文件访问速度
    //内存映射的起始地址 文件大小 映射区域可读 创建一个写入时复制的私有映射 文件描述符 偏移量
    //如果返回MAP_FAILED 生成错误响应
    int *mm_Ret=(int*)mmap(0,mm_File_Stat.st_size,PROT_READ,MAP_PRIVATE,src_Fd,0);
    if(*mm_Ret==-1)
    {
        Error_Content(buff,"File Not Found");
        return;
    }
    mm_File=(char*)mm_Ret;
    close(src_Fd);
    buff.Append("Content-length: "+to_string(mm_File_Stat.st_size)+"\r\n\r\n");
}



string Http_Response::Get_File_Type()
{
    //文件路径path中最后一个.的位置
    string::size_type index=rp_path.find_last_of('.');
    if(index==string::npos)
        return "text/plain";
    //从.开始提取后缀字符串
    string suffix=rp_path.substr(index);
    if(SUFFIX_TYPE.count(suffix)==1)
        return SUFFIX_TYPE.find(suffix)->second;
    return "text/plain";
}

void Http_Response::Error_Content(Buffer &buff, string message)
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(rp_code) == 1)
        status = CODE_STATUS.find(rp_code)->second;
    else
    {
        status = "Bad Request";
    }
    body += to_string(rp_code) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";
    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}
