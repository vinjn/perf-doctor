#pragma once

#include "cinder/osc/Osc.h"
#include "cinder/Log.h"

struct OscHelper
{
    static std::unique_ptr<cinder::osc::SenderUdp> createSender(const std::string& remoteIp, uint16_t remotePort)
    {
        auto sender = std::make_unique<cinder::osc::SenderUdp>(10000, remoteIp, remotePort);
        try
        {
            sender->bind();
            CI_LOG_I("sender->bind() " << remoteIp << ":" << remotePort);
        }
        catch (const cinder::Exception &ex)
        {
            CI_LOG_EXCEPTION("sender->bind() fails ", ex);
        }

        return sender;
    }

    static std::unique_ptr<cinder::osc::ReceiverUdp> createReceiver(uint16_t localPort)
    {
        auto receiver = std::make_unique<cinder::osc::ReceiverUdp>(localPort);
        try
        {
            receiver->bind();
            CI_LOG_I("sender->bind() " << localPort);
        }
        catch (const cinder::Exception &ex)
        {
            CI_LOG_EXCEPTION("receiver->bind() fails", ex);
        }

        receiver->listen([](asio::error_code ec, asio::ip::udp::endpoint ep) -> bool {
            if (ec) {
                CI_LOG_E("Error on listener: " << ec.message() << " Error Value: " << ec.value());
                return false;
            }
            else
                return true;
        });

        return receiver;

#if 0
        mOscReceiver->setListener("/start", [&](const osc::Message& message) {
            console() << message;
        });
#endif
    }
};
