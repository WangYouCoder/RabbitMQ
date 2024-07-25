#pragma once
#include "../mqcommon/logger.hpp"
#include "../mqcommon/msg.pb.h"
#include "../mqcommon/util.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <list>
#include <cassert>
using namespace WY;

namespace mq
{

using MessagePtr = std::shared_ptr<Message>;
#define DATAFILE_SUBFIX ".mqd"
#define TEMFILE_SUBFIX ".mqd.tmp"
class MessageMapper
{
public:
    // 创建消息所需要的文件
    MessageMapper(std::string &basedir, const std::string &qname)
    : _qname(qname)
    {
        if(basedir.back() != '/') basedir.push_back('/');
        _datafile = basedir + qname + DATAFILE_SUBFIX;
        _tempfile = basedir + qname + TEMFILE_SUBFIX;
        if(UtilFile(basedir).exists() == false)
        {
            assert(UtilFile::createDirectory(basedir));
        }
        createMsgFile();
    }

    // 创建存储消息的文件
    bool createMsgFile()
    {
        if(UtilFile(_datafile).exists() == true) return true;
        bool ret = UtilFile::createFile(_datafile);
        if(ret == false)
        {
            ELOG("创建文件 %s 失败\n", _datafile.c_str());
            return false;
        }
        return true;
    }

    // 删除消息文件
    void removeMsgFile()
    {
        UtilFile::removeFile(_datafile);
        UtilFile::removeFile(_tempfile);
    }

    // 向消息文件中插入数据
    bool insert(MessagePtr &msg)
    {
        return insert(_datafile, msg);
    }

    // 从文件中删除消息(但不是真正的删除，只是在文件中将有效标志位设置为0)
    bool remove(MessagePtr &msg)
    {
        msg->mutable_paylaod()->set_valid("0");
        std::string body = msg->paylaod().SerializeAsString();
        if(body.size() != msg->length())
        {
            ELOG("不能修改文件中的数据，长度不一致，会覆盖掉其他数据\n");
            return false;
        }
        UtilFile f(_datafile);
        bool ret = f.write(body.c_str(), msg->offset(), body.size());
        if(ret == false)
        {
            ELOG("向文件 %s 写入数据失败\n", _datafile.c_str());
            return false;
        }
        return true;
    }

    // 垃圾回收，真正的从文件中删除数据
    std::list<MessagePtr> gc()
    {
        std::list<MessagePtr> result;
        // 1. 加载出文件中所有的有效数据
        if(load(result) == false)
        {
            ELOG("加载有效数据失败\n");
            return result;
        }
        DLOG("垃圾回收，得到的有效消息数量: %ld", result.size());
        bool ret;
        // 2. 将有效数据，进行序列化存放到临时文件中
        UtilFile::createFile(_tempfile);
        for(auto &msg : result)
        {
            // DLOG("向临时文件中写入数据: %s", msg->paylaod().body().c_str());
            if(insert(_tempfile, msg) == false)
            {
                ELOG("向临时文件写入数据失败\n");
                return result;
            }
        }
        DLOG("垃圾回收后，向临时文件写入数据完毕，临时文件大小: %ld", UtilFile(_tempfile).size());
        // 3. 修改源文件
        if(UtilFile::removeFile(_datafile) == false)
        {
            ELOG("删除源文件失败\n");
            return result;
        }

        // 4. 修改临时文件名，为源文件名
        if(UtilFile(_tempfile).rename(_datafile) == false)
        {
            ELOG("修改文件名失败\n");
        }
        // 5. 返回新的有效数据
        return result;
    }
private:

    // 将文件中的有效数据读取出来，加载到内存中
    bool load(std::list<MessagePtr> &result)
    {
        size_t offset = 0, msg_size = 0;
        UtilFile f(_datafile);
        size_t fsize = f.size();
        // DLOG("fsize: %ld", fsize);
        while(offset < fsize)
        {
            bool ret = f.read((char*)&msg_size, offset, sizeof(size_t));
            if(ret == false)
            {
                ELOG("读取消息长度失败\n");
                return false;
            }
            // DLOG("offset: %ld", offset);
            // DLOG("msg_size: %ld", msg_size);
            offset += sizeof(size_t);
            std::string body(msg_size, '\0');
            ret = f.read(&body[0], offset, msg_size);
            if(ret == false)
            {
                ELOG("读取消息失败\n");
                return false;
            }
            // DLOG("body: %s", body.c_str());
            offset += msg_size;
            MessagePtr msgp = std::make_shared<Message>();
            msgp->mutable_paylaod()->ParseFromString(body);
            // msgp->ParseFromString(body);
            // 如果是无效消息直接跳过
            if(msgp->paylaod().valid() == "0") continue;
            // 有效就存储
            result.push_back(msgp);
        }
        return true;
    }

    // 向指定文件中插入数据
    bool insert(const std::string &filname, MessagePtr &msg/*结构化数据*/)
    {
        std::string body = msg->paylaod().SerializeAsString();
        UtilFile f(filname);
        size_t fsize = f.size();       // 获取文件长度
        size_t msg_size = body.size(); // 获取消息长度

        if(f.write((char*)&msg_size, fsize, sizeof(size_t)) == false)
        {
            ELOG("向队列数据文件写入数据长度失败\n");
        }
        
        bool ret = f.write(body.c_str(), fsize + sizeof(size_t), body.size());
        if(ret == false)
        {
            ELOG("向文件 %s 写入数据失败\n", filname.c_str());
            return false;
        }
        msg->set_offset(fsize + sizeof(size_t));
        msg->set_length(body.size());
        return true;
    }
private:
    std::string _qname;
    std::string _datafile;
    std::string _tempfile;
};

// 用来管理内存中队列的消息
class QueueMessage
{
public:
    using ptr = std::shared_ptr<QueueMessage>;
    // 
    QueueMessage(std::string &basedir, const std::string &qname)
        :_qname(qname), _mapper(basedir, qname), _valid_count(0), _total_count(0)
    {
       recovery();
    }

    void recovery()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _msgs = _mapper.gc();
        for(auto &msg : _msgs)
        {
            _durable_msgp.insert(std::make_pair(msg->paylaod().properties().id(), msg));
        }
        _valid_count = _total_count = _msgs.size();
    }

    // 将一个消息插入到队列中(内存级)，在这个类中初始化了一个指定队列
    bool insert(const BasicProperties *bp/*消息的属性*/, const std::string &body/*消息的内容*/, bool queue_durable)
    {
        // 1. 构造消息对象
        MessagePtr msg = std::make_shared<Message>();
        msg->mutable_paylaod()->set_body(body);
        if(bp != nullptr)
        {
            DeliveryMode mode = queue_durable ? bp->delivery_mode() : DeliveryMode::UNDURABLE;
            msg->mutable_paylaod()->mutable_properties()->set_id(bp->id());
            msg->mutable_paylaod()->mutable_properties()->set_delivery_mode(bp->delivery_mode());
            msg->mutable_paylaod()->mutable_properties()->set_routing_key(bp->routing_key());
        }
        else
        {
            DeliveryMode mode = queue_durable ? DeliveryMode::DURABLE : DeliveryMode::UNDURABLE;
            msg->mutable_paylaod()->mutable_properties()->set_id(Util::uuid());
            msg->mutable_paylaod()->mutable_properties()->set_delivery_mode(mode);
            msg->mutable_paylaod()->mutable_properties()->set_routing_key("");
        }

        std::unique_lock<std::mutex> lock(_mutex);
        // 2. 消息是否需要持久化
        if(msg->paylaod().properties().delivery_mode() == DeliveryMode::DURABLE)
        {
            msg->mutable_paylaod()->set_valid("1");
            // 3. 进行持久化存储
            if(_mapper.insert(msg) == false)
            {
                ELOG("持久化存储消息: %s 失败\n", body.c_str());
            }
            // DLOG("_vaild_count: %ld",_valid_count);
            _valid_count += 1;
            _total_count += 1;
            _durable_msgp.insert(std::make_pair(msg->paylaod().properties().id(), msg));
        }
        // 4. 内存管理
        _msgs.push_back(msg);
        return true;
    }

    // 获取队头元素
    MessagePtr front()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if(_msgs.size() == 0)
            return MessagePtr();
        // 从待推送消息中取出队头消息
        MessagePtr msgp = _msgs.front();
        _msgs.pop_front();
        // 同时为了避免推送失败，还需要存储到待确认消息中
        _waitack_msgp.insert(std::make_pair(msgp->paylaod().properties().id(), msgp));
        return msgp;
    }

    // 每次删除消息后，都需要判断是否需要进行垃圾回收
    // 从队列中删除消息，在这个类中初始化了一个指定队列
    bool remove(const std::string &msg_id)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _waitack_msgp.find(msg_id);
        if(it == _waitack_msgp.end())
        {
            DLOG("没有找到ID为: %s 的消息", msg_id.c_str());
            return true;
        }
        if(it->second->paylaod().properties().delivery_mode() == DeliveryMode::DURABLE)
        {
            _mapper.remove(it->second);
            _durable_msgp.erase(msg_id);
            _valid_count--;
            gc();
        }
        _waitack_msgp.erase(msg_id);
        return true;
    }

    // 获取内存中的当前队列的消息个数
    size_t getable_count()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _msgs.size();
    }

    // 获取内存中当前队列的有效消息个数
    size_t valid_count()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        // return _valid_count;
        return _durable_msgp.size();
    }

    // 获取内存中当前队列的所有消息个数
    size_t total_count()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _total_count;
    }

    // 获取内存中当前队列的所有待确认消息个数
    size_t waitack_count()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _waitack_msgp.size();
    }

    // 清理数据
    void clear()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _mapper.removeMsgFile();
        _msgs.clear();
        _durable_msgp.clear();
        _waitack_msgp.clear();
        _valid_count = 0;
        _total_count = 0;
    }
private:
    // 进行判断，是否需要进行垃圾回收
    bool gcCheck()
    {
        if(_total_count > 2000 && _valid_count * 10 / _total_count < 5)
            return true;
        
        return false;
    }

    // 垃圾回收(就是将文件中的有效消息转移到另一个文件中，并修改文件名)
    void gc()
    {
        // 1. 进行垃圾回收，获取到垃圾回收后的有效链表
        if(gcCheck() == false) return;
        // 2. 更新每一个消息的实际存储位置
        std::list<MessagePtr> msgs = _mapper.gc();  // 得到有效数据
        for(auto &msg : msgs)
        {
            auto it = _durable_msgp.find(msg->paylaod().properties().id());
            if(it == _durable_msgp.end())
            {
                DLOG("垃圾回收后，丢失了一条内存消息！！！");
                _msgs.push_back(msg);
                _durable_msgp.insert(std::make_pair(msg->paylaod().properties().id(), msg));
                continue;
            }
            it->second->set_offset(msg->offset());
            it->second->set_length(msg->length());
        }
        // 3. 更新当前的有笑消息的数量 && 总消息数量
        _valid_count = _total_count = msgs.size();
    }
private:
    std::mutex _mutex;
    std::string _qname;
    size_t _valid_count; // 有效消息数量
    size_t _total_count;
    MessageMapper _mapper;
    std::list<MessagePtr> _msgs; //待推送消息
    std::unordered_map<std::string /*消息ID*/, MessagePtr> _durable_msgp; // 持久化消息
    std::unordered_map<std::string /*消息ID*/, MessagePtr> _waitack_msgp; // 待确认消息
};

class MessageManager
{
public:
    using ptr = std::shared_ptr<MessageManager>;
    MessageManager(const std::string &basedir) : _basedir(basedir)
    {}
    void initQueueMessage(const std::string &qname)
    {
        QueueMessage::ptr qmp;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _queue_message.find(qname);
            if(it != _queue_message.end())
            {
                return;
            }
            qmp = std::make_shared<QueueMessage>(_basedir, qname);
            DLOG("queue_name: %s", qname.c_str());
            _queue_message.insert(std::make_pair(qname, qmp));
        }
        qmp->recovery();
    }
    void destroyQueueMessage(const std::string &qname)
    {
        QueueMessage::ptr qmp;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _queue_message.find(qname);
            if(it == _queue_message.end())
            {
                return;
            }
            qmp = it->second;
            _queue_message.erase(it);
        }
        qmp->clear();
    }
    bool insert(const std::string &qname, BasicProperties *bp, const std::string &body, bool queue_durable)
    {
        QueueMessage::ptr qmp;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _queue_message.find(qname);
            if(it == _queue_message.end())
            {
                ELOG("向队列 %s 新增消息失败，没有找到该队列\n", qname.c_str());
                return false;
            }
            qmp = it->second;
        }
        
        return qmp->insert(bp, body, queue_durable);
    }

    MessagePtr front(const std::string &qname)
    {
        QueueMessage::ptr qmp;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _queue_message.find(qname);
            if(it == _queue_message.end())
            {
                ELOG("向队列 %s 新增消息失败，没有找到该队列\n", qname.c_str());
                return MessagePtr();
            }
            qmp = it->second;
        }
        return qmp->front();
    }

    void ack(const std::string &qname, const std::string msg_id)
    {
        QueueMessage::ptr qmp;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _queue_message.find(qname);
            if(it == _queue_message.end())
            {
                ELOG("确认队列 %s 消息 %s 失败，没有找到该队列\n", qname.c_str(), msg_id.c_str());
                return ;
            }
            qmp = it->second;
        }
        qmp->remove(msg_id);
    }
    size_t getable_count(const std::string &qname)
    {
        QueueMessage::ptr qmp;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _queue_message.find(qname);
            if(it == _queue_message.end())
            {
                ELOG("获取队列 %s 总持久化消息数量失败，没有找到该队列\n", qname.c_str());
                return -1;
            }
            qmp = it->second;
        }
        return qmp->getable_count();
    }

    // 获取内存中当前队列的有效消息个数
    size_t valid_count(const std::string &qname)
    {
        QueueMessage::ptr qmp;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _queue_message.find(qname);
            if(it == _queue_message.end())
            {
                ELOG("获取队列 %s 有效消息数量失败，没有找到该队列\n", qname.c_str());
                return -1;
            }
            qmp = it->second;
        }
        return qmp->valid_count();
    }

    // 获取内存中当前队列的所有消息个数
    size_t total_count(const std::string &qname)
    {
        QueueMessage::ptr qmp;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _queue_message.find(qname);
            if(it == _queue_message.end())
            {
                ELOG("获取队列 %s 总数消息数量失败，没有找到该队列\n", qname.c_str());
                return 0;
            }
            qmp = it->second;
        }
        return qmp->total_count();
    }

    // 获取内存中当前队列的所有待确认消息个数
    size_t waitack_count(const std::string &qname)
    {
         QueueMessage::ptr qmp;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _queue_message.find(qname);
            if(it == _queue_message.end())
            {
                ELOG("获取队列 %s 确认消息数量失败，没有找到该队列\n", qname.c_str());
                return -1;
            }
            qmp = it->second;
        }
        return qmp->waitack_count();
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        for(auto &qmsg : _queue_message)
        {
            qmsg.second->clear();
        }
    }
private:
    std::mutex _mutex;
    std::string _basedir;
    std::unordered_map<std::string/*队列名称*/, QueueMessage::ptr> _queue_message;
};
}