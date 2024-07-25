#pragma once
#include "../mqcommon/logger.hpp"
#include "../mqcommon/msg.pb.h"
#include "../mqcommon/util.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

class Binding
{
public:
    using ptr = std::shared_ptr<Binding>;
    std::string exchange_name;
    std::string msgqueue_name;
    std::string binding_key;

    Binding(){}
    Binding(const std::string &ename, const std::string &qname, const std::string &key)
        : exchange_name(ename), msgqueue_name(qname), binding_key(key)
    {}
};

// 队列与绑定信息是一一对应的
using MagQueueBindingMap = std::unordered_map<std::string, Binding::ptr>;
// 交换机可以给多个队列发送消息，
// 如果不理解为什么使用MagQueueBindingMap，可以把它看成一个vector，
// 一个交换机对应多个队列，只不过这里多存了每个队列的绑定信息(用于删除交换机时，可以找得到对应的队列，删除队列相关的绑定信息)
using ExchangeBindingMap = std::unordered_map<std::string, MagQueueBindingMap>;
class BindingMapper
{
public:
    // 创建绑定所需要的数据库文件 && 创建表
    BindingMapper(const std::string &dbfile) :_handler(dbfile)
    {
        std::string path = UtilFile::parentDirectory(dbfile);
        UtilFile::createDirectory(path);
        assert(_handler.open());
        createTable();
    }

    // 创建表
    void createTable()
    {
        std::stringstream sql;
        sql << "create table if not exists binding_table(";
        sql << "exchange_name varchar(32),";
        sql << "msgqueue_name varchar(32),";
        sql << "binding_key varchar(128));";
        bool ret = _handler.exec(sql.str(), nullptr, nullptr);
        if(ret == false)
        {
            ELOG("创建绑定数据库表失败\n");
            abort();
        }
    }

    // 删除表
    void removeTable()
    {
        #define DROP_TABLE "drop table if exists binding_table;"
        bool ret = _handler.exec(DROP_TABLE, nullptr, nullptr);
        if(ret == false)
        {
            ELOG("删除绑定数据库表失败\n");
            abort();
        }
    }

    // 向《绑定数据库》中插入绑定信息
    bool insert(Binding::ptr binding)
    {
        std::stringstream sql;
        sql << "insert into binding_table values (";
        sql << "'" << binding->exchange_name << "',";
        sql << "'" << binding->msgqueue_name << "',";
        sql << "'" << binding->binding_key << "');";
        bool ret = _handler.exec(sql.str(), nullptr, nullptr);
        if(ret == false)
        {
            ELOG("插入绑定数据失败\n");
            return false;
        }
        return true;
    }

    // 从数据库中移除绑定数据库中指定交换机和队列的绑定信息
    void remove(const std::string &ename, const std::string &qname)
    {
        std::stringstream sql;
        sql << "delete from binding_table where ";
        sql << "exchange_name='" << ename << "' and ";
        sql << "msgqueue_name='" << qname << "'";
        bool ret = _handler.exec(sql.str(), nullptr, nullptr);
        if(ret == false)
        {
            ELOG("删除绑定数据失败\n");
        }
    }

    // 从数据库中移除所有交换机的绑定信息
    void removeExchangeBindings(const std::string &ename)
    {
        std::stringstream sql;
        sql << "delete from binding_table where ";
        sql << "exchange_name='" << ename << "';";
        bool ret = _handler.exec(sql.str(), nullptr, nullptr);
        if(ret == false)
        {
            ELOG("删除交换机的绑定数据失败\n");
        }
    }

    // 从数据库中移除所有队列的绑定信息
    void removeMsgQueueBindings(const std::string &qname)
    {
        std::stringstream sql;
        sql << "delete from binding_table where ";
        sql << "msgqueue_name='" << qname << "';";
        bool ret = _handler.exec(sql.str(), nullptr, nullptr);
        if(ret == false)
        {
            ELOG("删除队列的绑定数据失败\n");
        }
    }
    
    // 将数据库中的数据加载到内存
    ExchangeBindingMap recovery()
    {
        ExchangeBindingMap result;
        std::string sql = "select exchange_name, msgqueue_name, binding_key from binding_table;";
        _handler.exec(sql, selectCallback, &result);
        return result;
    }
private:
    // exchange -> exchange, queueMap     queueMap -> queue, binding
    static int selectCallback(void* arg/*回调函数中的第三个参数*/, int col/*行*/, char **row/*列*/, char **fields)
    {
        ExchangeBindingMap *result = (ExchangeBindingMap*) arg;
        Binding::ptr bp = std::make_shared<Binding>(row[0], row[1], row[2]);
        // 为了防止交换机相关的绑定信息已经存在，因此不能直接创建队列映射，进行添加，这样会进行覆盖历史数据
        // 因此需要先获取交换机对应的映射对象，往里面添加数据
        // 但是，若这时候没有交换机对应的映射信息，因此这里的获取要使用引用(会保证不存在则创建)
        MagQueueBindingMap &mqbp = (*result)[bp->exchange_name];
        mqbp.insert(std::make_pair(bp->msgqueue_name, bp));

        return 0;
    }
private:
    SqliteHelper _handler;
};

class BindingManager
{
public:
    using ptr = std::shared_ptr<BindingManager>;

    // 当外界实例化对象时，会自动将数据库中的文件加载出来
    BindingManager(const std::string &dbfile) : _mapper(dbfile)
    {
        _bindings = _mapper.recovery();
    }

    // 将相应的交换机和队列进行绑定
    bool bind(const std::string &ename, const std::string &qname, const std::string &key, bool durable/*是否需要进行存储到数据库中*/)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _bindings.find(ename);
        if(it != _bindings.end() && it->second.find(qname) != it->second.end())
        {
            return true;
        }

        Binding::ptr bp = std::make_shared<Binding>(ename, qname, key);
        if(durable)
        {
            bool ret = _mapper.insert(bp);
            if(ret == false) return false;
        }
        auto &qbmap = _bindings[ename];
        qbmap.insert(std::make_pair(qname, bp));
        return true;
    }

    // 移除指定交换机和队列的绑定信息
    void unBind(const std::string &ename, const std::string &qname)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto eit = _bindings.find(ename);
        if(eit == _bindings.end()) return;
        auto qit = eit->second.find(qname);
        if(qit == eit->second.end()) return;    
        _mapper.remove(ename, qname);
        _bindings[ename].erase(qname);
    }

    // 移除所有交换机的绑定信息
    void removeExchangeBinding(const std::string &ename)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _mapper.removeExchangeBindings(ename);
        _bindings.erase(ename);
    }

    // 移除所有队列的绑定信息
    void removeMasQueueBinding(const std::string &qname)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _mapper.removeMsgQueueBindings(qname);
        for(auto start = _bindings.begin(); start != _bindings.end(); ++start)
        {
            start->second.erase(qname);
        }
    }

    // 得到指定交换机绑定的所有队列信息
    MagQueueBindingMap getExchangeBinding(const std::string &ename)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto eit = _bindings.find(ename);
        if(eit == _bindings.end())
        {   
            return MagQueueBindingMap();
        }
        return eit->second;
    }

    // 得到指定交换机和队列的绑定信息
    Binding::ptr getBinding(const std::string &ename, const std::string &qname)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto eit = _bindings.find(ename);
        if(eit == _bindings.end())
        {   
            return Binding::ptr();
        }
        auto qit = eit->second.find(qname);
        if(qit == eit->second.end())
        {
            return Binding::ptr();
        }
        return qit->second;
    }

    // 判断某一交换机和队列是否存在绑定
    bool exists(const std::string &ename, const std::string &qname)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto eit = _bindings.find(ename);
        if(eit == _bindings.end())
        {   
            return false;
        }
        auto qit = eit->second.find(qname);
        if(qit == eit->second.end())
        {
            return false;
        }
        return true;
    }

    // 获得绑定数量
    size_t size()
    {
        size_t total_size = 0;
        std::unique_lock<std::mutex> lock(_mutex);
        for(auto start = _bindings.begin(); start != _bindings.end(); ++start)
        {
            total_size += start->second.size();
        }
        return total_size;
    }

    // 清理数据
    void clear()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _mapper.removeTable();
        _bindings.clear();
    }
private:
    std::mutex _mutex;      // 加锁
    BindingMapper _mapper;  // 用于进行持久化操作
    ExchangeBindingMap _bindings;   // 用来管理所有交换机所绑定的队列 --- map结构
                                    // using ExchangeBindingMap = std::unordered_map<std::string, MagQueueBindingMap>;

                                    // 队列与绑定信息是一一对应的
                                    // using MagQueueBindingMap = std::unordered_map<std::string, Binding::ptr>;

                                    // 交换机可以给多个队列发送消息，
                                    // 如果不理解为什么使用MagQueueBindingMap，可以把它看成一个vector，
                                    // 一个交换机对应多个队列，只不过这里多存了每个队列的绑定信息
                                    // (用于删除交换机时，可以找得到对应的队列，删除队列相关的绑定信息)
};