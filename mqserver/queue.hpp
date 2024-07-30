#pragma once
#include "../mqcommon/logger.hpp"
#include "../mqcommon/msg.pb.h"
#include "../mqcommon/util.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

class MsgQueue
{
public:
    std::string name;
    bool durable;
    bool exclusive;  // 是否独占
    bool auto_delete;
    google::protobuf::Map<std::string, std::string> args;

    MsgQueue(const std::string &qname, bool qdurable, bool qexclusive, bool qauto_delete, const google::protobuf::Map<std::string, std::string> &qargs)
        : name(qname), durable(qdurable), exclusive(qexclusive), auto_delete(qauto_delete), args(qargs)
    {}
    MsgQueue(){}

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
        if(!result.empty())
            result.pop_back();
        return result;
    }

    using ptr = std::shared_ptr<MsgQueue>;
};

// 队列持久化数据管理类 --- 存储再sqlite数据库中
class MsgQueueMapper
{
public:
    // 传入数据库的文件名及目录，在构造函数中创建出相对应的数据库文件
    MsgQueueMapper(const std::string &dbfile) : _handler(dbfile)
    {
        std::string path = UtilFile::parentDirectory(dbfile);
        UtilFile::createDirectory(path);
        assert(_handler.open());
        createTable();
    }
    
    void createTable()
    {
        // #define CREATE_TABLE "create table if not exists queue_table(\
        //     name varchar(32) primary key, durable int, exclusive int, auto_delete int, args varchar(128));"
        std::stringstream sql;
        sql << "create table if not exists queue_table( name varchar(32) primary key, durable int, exclusive int, auto_delete int, args varchar(128));";
        bool ret = _handler.exec(sql.str(), nullptr, nullptr);
        if(ret == false)
        {
            ELOG("创建队列数据库表失败\n");
            abort();
        }
    }
    void removeTable()
    {
        // #define DROP_TABLE "drop table if exists queue_table;"
        std::stringstream sql;
        sql << "drop table if exists queue_table;";
        bool ret = _handler.exec(sql.str(), nullptr, nullptr);
        if(ret == false)
        {
            ELOG("删除队列数据库表失败\n");
            abort();
        }
    }
    // 将一个队列数据插入到数据库中，进行持久化保存
    bool insert(MsgQueue::ptr &queue)
    {
        // #define INSERT_SQL "insert into queue_table values ('%s', '%d', '%d', '%d', '%s');"
        // std::string args_str = queue->getArgs();
        // char sql[4096] = {0};
        // sprintf(sql, INSERT_SQL, queue->name.c_str(), queue->durable, queue->exclusive, queue->auto_delete, args_str.c_str());
        std::stringstream sql;
        sql << "insert into queue_table values (";
        sql << "'" << queue->name << "',";
        sql << queue->durable << ",";
        sql << queue->exclusive << ",";
        sql << queue->auto_delete << ",";
        sql << "'" << queue->getArgs() << "');";
        bool ret = _handler.exec(sql.str(), nullptr, nullptr);
        // bool ret = _handler.exec(sql, nullptr, nullptr);
        if(ret == false)
        {
            ELOG("插入队列数据失败\n");
        }
        return ret;
    }
    // 从数据库中删除一个队列数据，去掉持久化
    void remove(const std::string &name)
    {
        std::stringstream ss;
        ss << "delete from queue_table where name=";
        ss << "'" << name << "'";
        bool ret = _handler.exec(ss.str(), nullptr, nullptr);
        if(ret == false)
        {
            ELOG("删除队列数据失败\n");
        }
    }
    // MsgQueue::ptr getOne(const std::string &name);

    // 用于将数据库文件中的一个队列数据加载到内存中
    using MsgQueueeMap = std::unordered_map<std::string, MsgQueue::ptr>;
    MsgQueueeMap recovery()
    {
        MsgQueueeMap result;
        std::string sql;
        sql = "select name, durable, exclusive, auto_delete, args from queue_table;";
        _handler.exec(sql, selectCallback, &result);  // 一次获取一行数据，在exec内部会循环调用回调函数，直到获得的数据为空
        return result;
    }
private:
    // 提供给recovery函数，将一个队列数据加载到内存时，需要先有一个队列对象来存放它的相关属性
    static int selectCallback(void* arg/*回调函数中的第三个参数*/, int col/*行*/, char **row/*列*/, char **fields)
    {
        MsgQueueeMap *result = (MsgQueueeMap*) arg;
        auto mqp = std::make_shared<MsgQueue>();
        mqp->name = row[0];
        mqp->durable = (bool)std::stoi(row[1]);     // durable
        mqp->exclusive = (bool)std::stoi(row[2]);   // exclusive
        mqp->auto_delete = (bool)std::stoi(row[3]); // auto_delete
        if(row[4])
            mqp->setArgs(row[4]);
        
        result->insert(std::make_pair(mqp->name, mqp));
        return 0;
    }
private:
    SqliteHelper _handler;
};

using QueueMap = std::unordered_map<std::string, MsgQueue::ptr>;

// 队列数据内存管理类
class MsgQueueManager
{
public:
    using ptr = std::shared_ptr<MsgQueueManager>;
    // 指明需要加载哪一个数据库文件中的队列，并得到一个队列数据(数据是map name -> exchange)
    MsgQueueManager(const std::string &dbfile) :_mapper(dbfile)
    {
        _msg_queue = _mapper.recovery();
    }

    // 用于声明一个队列，如果该队列的durable为真，则还需要保存到数据库中
    bool declareQueue(const std::string &name, bool durable, bool exclusive, bool auto_delete, 
        const google::protobuf::Map<std::string, std::string> &args)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _msg_queue.find(name);
            if(it != _msg_queue.end())
            {
                // 队列已经存在，直接返回，不需要重复新增
                return true;
            }
            auto mqp = std::make_shared<MsgQueue>(name, durable, exclusive, auto_delete, args);
            if(durable == true)
            {
                // 保存到数据库中
                _mapper.insert(mqp);
            }
            // 存储到内存中
            _msg_queue.insert(std::make_pair(name, mqp));
            return true;
        }

    // 删除一个队列
    void deleteQueue(const std::string &name)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _msg_queue.find(name);
        if(it == _msg_queue.end())
        {
            // 队列不存在，直接返回
            return;
        }
        if(it->second->durable == true) _mapper.remove(name);
        _msg_queue.erase(name);

    }   
    // 获取一个队列数据
    MsgQueue::ptr selectQueue(const std::string &name)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _msg_queue.find(name);
        if(it == _msg_queue.end())
        {
            // 队列不存在，直接返回
            return MsgQueue::ptr();
        }
        return it->second;
    }

    QueueMap getAllQueue()
    {
        return _msg_queue;
    }

    bool exists(const std::string &name)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _msg_queue.find(name);
        if(it == _msg_queue.end())
        {
            // 队列不存在，直接返回
            return false;
        }
        return true;
    }

    size_t size()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _msg_queue.size();
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _mapper.removeTable();
        _msg_queue.clear();
    }
private:
    std::mutex _mutex;
    MsgQueueMapper _mapper;
    std::unordered_map<std::string/*队列名称*/, MsgQueue::ptr> _msg_queue;
};