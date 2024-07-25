#pragma once
#include "../mqcommon/logger.hpp"
#include "../mqcommon/msg.pb.h"
#include "../mqcommon/util.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <google/protobuf/map.h>
using namespace WY;

// 交换机类
class Exchange
{
public:
    std::string name;
    ExchangeType type;
    bool durable;
    bool auto_delete;
    google::protobuf::Map<std::string, std::string> args;

    Exchange(const std::string &_name, ExchangeType _type, bool _durable, bool _auto_delete, 
        const google::protobuf::Map<std::string, std::string> &_args)
        : name(_name), type(_type), durable(_durable), auto_delete(_auto_delete), args(_args) 
    {}

    Exchange(){}

    // 用于初始化args成员变量(其他参数)
    void setArgs(const std::string &str_args) // 内部解析str_args字符串，将内容存储到成员中
    {
        //key=val&key=val
        std::vector<std::string> sub_args;
        Util::split(str_args, "&", sub_args);
        for(auto &str : sub_args)
        {
            size_t pos = str.find("=");
            std::string key = str.substr(0, pos);
            std::string val = str.substr(pos + 1);
            args[key] = val;
        }
    }

    // 将args中的内容转为字符串
    std::string getArgs() // 将args中的内容进行序列化，返回一个字符串
    {
        std::string result;
        for(auto i = args.begin(); i != args.end(); i++)
        {
            result += i->first + "=" + i->second + "&";
        }
        result.pop_back();
        return result;
    }

    using ptr = std::shared_ptr<Exchange>;
};

// 交换机持久化数据管理类 --- 存储再sqlite数据库中
class ExchangeMapper
{
public:
    // 传入数据库的文件名及目录，在构造函数中创建出相对应的数据库文件
    ExchangeMapper(const std::string &dbfile) : _handler(dbfile)
    {
        std::string path = UtilFile::parentDirectory(dbfile);
        UtilFile::createDirectory(path);
        assert(_handler.open());
        createTable();
    }
    
    void createTable()
    {
        // #define CREATE_TABLE "create table if not exists exchange_table(\
        //     name varchar(20) primary key, type int, durable int, auto_delete int, args varchar(128));"
        std::stringstream sql;
        sql << "create table if not exists exchange_table(\
            name varchar(20) primary key, type int, durable int, auto_delete int, args varchar(128));";
        bool ret = _handler.exec(sql.str(), nullptr, nullptr);
        if(ret == false)
        {
            ELOG("创建交换机数据库表失败\n");
            abort();
        }
    }
    void removeTable()
    {
        // #define DROP_TABLE "drop table if exists exchange_table;"
        std::stringstream sql;
        sql << "drop table if exists exchange_table;";
        bool ret = _handler.exec(sql.str(), nullptr, nullptr);
        if(ret == false)
        {
            ELOG("删除交换机数据库表失败\n");
            abort();
        }
    }
    // 将一个交换机数据插入到数据库中，进行持久化保存
    void insert(Exchange::ptr &exp)
    {
        #define INSERT_SQL "insert into exchange_table values ('%s', '%d', '%d', '%d', '%s');"
        std::string args_str = exp->getArgs();
        char sql[4096] = {0};
        sprintf(sql, INSERT_SQL, exp->name.c_str(), exp->type, exp->durable, exp->auto_delete, args_str.c_str());
        bool ret = _handler.exec(sql, nullptr, nullptr);
        if(ret == false)
        {
            ELOG("插入交换机数据失败\n");
        }
    }
    // 从数据库中删除一个交换机数据，去掉持久化
    void remove(const std::string &name)
    {
        std::stringstream ss;
        ss << "delete from exchange_table where name=";
        ss << "'" << name << "'";
        bool ret = _handler.exec(ss.str(), nullptr, nullptr);
        if(ret == false)
        {
            ELOG("删除交换机数据失败\n");
        }
    }
    // Exchange::ptr getOne(const std::string &name);

    // 用于将数据库文件中的一个交换机数据加载到内存中
    using ExchangeMap = std::unordered_map<std::string, Exchange::ptr>;
    ExchangeMap recovery()
    {
        ExchangeMap result;
        std::string sql;
        sql = "select name, type, durable, auto_delete, args from exchange_table;";
        _handler.exec(sql, selectCallback, &result);
        return result;
    }
private:
    // 提供给recovery函数，将一个交换机数据加载到内存时，需要先有一个交换机对象来存放它的相关属性
    static int selectCallback(void* arg/*回调函数中的第三个参数*/, int col/*行*/, char **row/*列*/, char **fields)
    {
        ExchangeMap *result = (ExchangeMap*) arg;
        auto exp = std::make_shared<Exchange>();
        exp->name = row[0];
        exp->type = (ExchangeType)std::stoi(row[1]);
        exp->durable = (bool)std::stoi(row[2]);
        exp->auto_delete = (bool)std::stoi(row[3]);
        if(row[4])
            exp->setArgs(row[4]);
        
        result->insert(std::make_pair(exp->name, exp));
        return 0;
    }
private:
    SqliteHelper _handler;
};

// 交换机数据内存管理类
class ExchangeManager
{
public:
    using ptr = std::shared_ptr<ExchangeManager>;
    // 指明需要加载哪一个数据库文件中的交换机，并得到一个交换机数据(数据是map name -> exchange)
    ExchangeManager(const std::string &dbfile) :_mapper(dbfile)
    {
        _exchange = _mapper.recovery();
    }

    // 用于声明一个交换机，如果该交换机的durable为真，则还需要保存到数据库中
    bool declareExchange(const std::string &name, ExchangeType type, bool durable, bool auto_delete, 
        const google::protobuf::Map<std::string, std::string> &args)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _exchange.find(name);
            if(it != _exchange.end())
            {
                // 交换机已经存在，直接返回，不需要重复新增
                return false;
            }
            auto exp = std::make_shared<Exchange>(name, type, durable, auto_delete, args);
            if(durable == true)
            {
                // 保存到数据库中
                _mapper.insert(exp);
            }
            // 存储到内存中
            _exchange.insert(std::make_pair(name, exp));
            return true;
        }

    // 删除一个交换机
    void deleteExchange(const std::string &name)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _exchange.find(name);
        if(it == _exchange.end())
        {
            // 交换机不存在，直接返回
            return;
        }
        if(it->second->durable == true) _mapper.remove(name);
        _exchange.erase(name);

    }   
    // 获取一个交换机数据
    Exchange::ptr selectExchange(const std::string &name)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _exchange.find(name);
        if(it == _exchange.end())
        {
            // 交换机不存在，直接返回
            return Exchange::ptr();
        }
        return it->second;
    }
    bool exists(const std::string &name)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _exchange.find(name);
        if(it == _exchange.end())
        {
            // 交换机不存在，直接返回
            return false;
        }
        return true;
    }

    size_t size()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _exchange.size();
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _mapper.removeTable();
        _exchange.clear();
    }
private:
    std::mutex _mutex;
    ExchangeMapper _mapper;
    std::unordered_map<std::string/*交换机名称*/, Exchange::ptr/*交换机名称所对应的对象*/> _exchange;
};