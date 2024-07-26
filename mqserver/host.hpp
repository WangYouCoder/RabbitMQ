#pragma once
#include "binding.hpp"
#include "exchange.hpp"
#include "message.hpp"
#include "queue.hpp"

class VirtualHost
{
public:
    using ptr = std::shared_ptr<VirtualHost>;
    VirtualHost(const std::string hostname, const std::string &basedir, const std::string &dbfile) : 
        _host_name(hostname),
        _bmp(std::make_shared<BindingManager>(dbfile)),
        _emp(std::make_shared<ExchangeManager>(dbfile)),
        _mqmp(std::make_shared<MsgQueueManager>(dbfile)),
        _mmp(std::make_shared<mq::MessageManager>(basedir))
    {
        // 恢复所有队列的消息
        QueueMap qm = _mqmp->getAllQueue();
        for(auto &q : qm)
        {
            _mmp->initQueueMessage(q.first);
        }
    }

    bool declareExchange(const std::string &ename, ExchangeType type, bool durable, bool auto_delete, 
        const google::protobuf::Map<std::string, std::string> &args)
    {
        return _emp->declareExchange(ename, type, durable, auto_delete, args);
    }
    void deleteExchange(const std::string &ename)
    {
        _bmp->removeExchangeBinding(ename);
        _emp->deleteExchange(ename);
    }
    bool existsExchange(const std::string &ename)
    {
        return _emp->exists(ename);
    }
    Exchange::ptr selectExchange(const std::string &ename)
    {
        return _emp->selectExchange(ename);
    }

    bool declareQueue(const std::string &qname, bool durable, bool exclusive, bool auto_delete, 
        const google::protobuf::Map<std::string, std::string> &args)
    {
        _mmp->initQueueMessage(qname);
       return _mqmp->declareQueue(qname, durable, exclusive, auto_delete, args);
    }
    void deleteQueue(const std::string &qname)
    {
        _bmp->removeMasQueueBinding(qname);
        _mmp->destroyQueueMessage(qname);
        return _mqmp->deleteQueue(qname);
    }
    bool existsQueue(const std::string &qname)
    {
        return _mqmp->exists(qname);
    }

    bool bind(const std::string &ename, const std::string &qname, const std::string &key)
    {
        Exchange::ptr ep = _emp->selectExchange(ename);
        if(ep.get() == nullptr)
        {
            ELOG("进行绑定失败，交换机 %s : 不存在", ename.c_str());
            return false;
        }
        MsgQueue::ptr mqp = _mqmp->selectQueue(qname);
        if(mqp.get() == nullptr)
        {
            ELOG("进行绑定失败，队列 %s : 不存在", qname.c_str());
            return false;
        }

        return _bmp->bind(ename, qname, key, ep->durable && mqp->durable);
    }
    void unbind(const std::string &ename, const std::string &qname)
    {
        _bmp->unBind(ename, qname);
    }
    MagQueueBindingMap exchangeBindings(const std::string &ename)
    {
        return _bmp->getExchangeBinding(ename);
    }
    bool existsBinding(const std::string &ename, const std::string &qname)
    {
        return _bmp->exists(ename, qname);
    }

    bool basicPublish(const std::string &qname, BasicProperties *bp, const std::string &body)
    {
        MsgQueue::ptr mqp = _mqmp->selectQueue(qname);
        if(mqp.get() == nullptr)
        {
            ELOG("推送消息失败，队列 %s : 不存在", qname.c_str());
            return false;
        }

        DeliveryMode mode = mqp->durable ? DeliveryMode::DURABLE : DeliveryMode::UNDURABLE;
        return _mmp->insert(qname, bp, body, mode);
    }
    mq::MessagePtr basicConsume(const std::string &qname)
    {
        return _mmp->front(qname);
    }
    void basicAck(const std::string &qname, const std::string msg_id)
    {
       _mmp->ack(qname, msg_id); 
    }

    QueueMap allQueues()
    {
        return _mqmp->getAllQueue();
    }

    void clear()
    {
        _emp->clear();
        _mqmp->clear();
        _bmp->clear();
        _mmp->clear();
    }
private:
    BindingManager::ptr _bmp;
    ExchangeManager::ptr _emp;
    MsgQueueManager::ptr _mqmp;
    mq::MessageManager::ptr _mmp;
    std::string _host_name;
};