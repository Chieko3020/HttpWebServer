#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap

#include "../buffer/buffer.h"
#include "../log/log.h"

class Http_Response {
public:
    Http_Response();
    ~Http_Response();
    //源目录 路径 是否保持连接 状态码
    void Init(const std::string& src_Dir, std::string& path, bool is_Keep_Alive = false, int code = -1);
    void Make_Response(Buffer& buff);
    void Un_map_File();//释放文件映射
    char* File();//映射的文件指针
    size_t File_Len() const;//映射的文件大小
    void Error_Content(Buffer& buff, std::string message);//生成错误消息
    int Code() const//状态码 
    { 
        return rp_code;
    }

private:
    void Add_State_Line(Buffer &buff);//添加状态行
    void Add_Header(Buffer &buff);//添加响应头部
    void Add_Content(Buffer &buff);//添加响应正文

    void Error_Html();//根据状态码设置错误页面
    std::string Get_File_Type();//返回文件的MIME类型

    int rp_code;//状态码
    bool rp_is_Keep_Alive;

    std::string rp_path;
    std::string rp_src_Dir;
    
    char* mm_File;//映射的文件指针 
    struct stat mm_File_Stat;//文件状态信息

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;//后缀类型
    static const std::unordered_map<int, std::string> CODE_STATUS;//状态码
    static const std::unordered_map<int, std::string> CODE_PATH;//错误页面路径
};


#endif //HTTP_RESPONSE_H

