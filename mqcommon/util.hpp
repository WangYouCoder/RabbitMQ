#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <sqlite3.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <cerrno>
#include "logger.hpp"
class Util
{
public:
    static size_t split(const std::string& str, const std::string& sep, std::vector<std::string> &result)
    {
        if(str.size() == 0 || sep.size() == 0) return 0;
        int pos = 0, idx = 0;
        while(idx < str.size())
        {
            pos = str.find(sep, idx);
            if(pos == std::string::npos)
            {
                result.push_back(str.substr(idx));
                return result.size();
            }

            if(pos == idx)
            {
                idx += sep.size();
                continue;
            }
            result.push_back(str.substr(idx, pos - idx));
            idx = pos + sep.size();
        }
        return result.size();
    }

    static std::string uuid()
    {
        std::random_device rd;
        std::mt19937_64 gernator(rd());
        std::uniform_int_distribution<int> distribution(0, 255);
        std::stringstream ss;
        for(int i = 0; i < 8; i++)
        {
            ss << std::setw(2) << std::setfill('0') << std::hex << distribution(gernator);
            if(i == 3 || i == 5 || i == 7)
                ss << "-";
        }

        static std::atomic<size_t> seq(1);
        size_t num = seq.fetch_add(1);
        for(int i = 7; i >= 0; i--)
        {
            ss << std::setw(2) << std::setfill('0') << std::hex << (num>>(i*8) & 0xff);
            if(i == 6) ss << "-";
        }
        return ss.str();
    }
};


class SqliteHelper
{
public:
    typedef int(*SqliteCallback)(void*, int, char**, char**);
    SqliteHelper(const std::string &dbfile) : _dbfile(dbfile)
    {}
    
    bool open(int safe_level = SQLITE_OPEN_FULLMUTEX)
    {
        // int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs );
        int ret = sqlite3_open_v2(_dbfile.c_str(), &_handler, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | safe_level, nullptr);
        if(ret != SQLITE_OK)
        {
            ELOG("打开数据库失败: %s", sqlite3_errmsg(_handler));
            return false;
        }
        return true;
    }

    bool exec(const std::string &sql, SqliteCallback cb, void *arg)
    {
        // int sqlite3_exec(sqlite3*, char *sql, int (*callback)(void*,int,char**,char**), void* arg, char **err)
        int ret = sqlite3_exec(_handler, sql.c_str(), cb, arg, nullptr);
        if(ret != SQLITE_OK)
        {
            ELOG("%s 执行失败: %s", sql.c_str(), sqlite3_errmsg(_handler));
            return false;
        }
        return true;
    }
    void close()
    {
        // int sqlite3_close_v2(sqlite3*);
        if(_handler)
            sqlite3_close_v2(_handler);

    }
private:
    std::string _dbfile;
    sqlite3 *_handler;
};

class UtilFile
{
public:
    UtilFile(const std::string& filename) : _filename(filename)
    {}
    bool exists()
    {
        struct stat st;
        return stat(_filename.c_str(), &st) == 0;
    }
    size_t size()
    {   
        struct stat st;
        if(stat(_filename.c_str(), &st) != 0)
            return 0;
        
        return st.st_size; 
    }
    bool read(std::string& body)
    {
        size_t fsize = this->size();
        body.resize(fsize);
        return read(&body[0], 0, fsize);
    }
    bool read(char* body, size_t offset, size_t len)
    {
        std::ifstream in(_filename, std::ios::binary | std::ios::in);
        if(!in.is_open())
        {
            ELOG("打开文件 %s 失败\n",_filename.c_str());
            return false;
        }
        in.seekg(offset, std::ios::beg);
        in.read(body, len);
        if(in.good() == false)
        {
            ELOG("读取 %s 文件失败\n",_filename.c_str());
            in.close();
            return false;
        }
        in.close();
        return true;
    }
    bool write(const std::string &body)
    {
        return write(body.c_str(), 0, body.size());
    }
    bool write(const char* body, size_t offset, size_t len)
    {
        std::fstream io(_filename, std::ios::in | std::ios::out | std::ios::binary);
        if(!io.is_open())
        {
            ELOG("打开 %s 文件失败\n", _filename.c_str());
            return false;
        }
        io.seekg(offset, std::ios::beg);

        io.write(body, len);
        if(io.good() == false)
        {
            ELOG("写 %s 文件失败\n", _filename.c_str());
            io.close();
            return false;
        }
        io.close();
        return true;
    }

    bool rename(const std::string& newname)
    {
        return (::rename(_filename.c_str(), newname.c_str()) == 0);
    }

    static bool createFile(const std::string &filename)
    {
        std::fstream in(filename, std::ios::binary | std::ios::out);
        if(!in.is_open())
        {
            ELOG("打开 %s 文件失败\n",filename.c_str());
            return false;
        }
        in.close();
        return true;
    }
    static bool removeFile(const std::string &filename)
    {
        return (::remove(filename.c_str()) == 0);
    }
    static bool createDirectory(const std::string path)
    {   
        size_t idx = 0;
        while(idx < path.size())
        {
            size_t pos = path.find("/", idx);
            if(pos == std::string::npos)
            {
                return (mkdir(path.c_str(), 0775) == 0);
            }
            std::string subpath = path.substr(0, pos);
            int ret = mkdir(subpath.c_str(), 0775);
            if(ret != 0 && errno != EEXIST)
            {
                ELOG("创建 %s 目录失败\n",subpath.c_str());
                return false;
            }
            idx = pos + 1;
        }
        return true;
    }
    static bool removeDirectory(const std::string path)
    {
        std::string cmd = "rm -rf " + path;
        return (system(cmd.c_str()) != -1);
    }
    static std::string parentDirectory(const std::string& filepath)
    {
        size_t pos = filepath.find_last_of("/");
        if(pos == std::string::npos)
        {
            return "./";
        }
        return filepath.substr(0, pos);
    }
private:
    std::string _filename;
};