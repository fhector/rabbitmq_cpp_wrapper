#include <iostream>
#include <cstdio>
#include <string>
#include <cstring>
#include <cstdint>
#include <vector>
#include <map>

#include "../RabbitMQ.h"


bool setupChannel(AMQP::RabbitChannelPtr pChannel, void* pArg) {

	if (!pChannel) {
		std::cout << "nullptr Error!" << std::endl;
		return false;
	}

    if(pChannel->declareExchange("hello-exchange", "direct", false/*passive*/, true/*durable*/, false/*auto_delete*/) < 0) {
        std::cout << "declareExchange Error!" << std::endl;
        return false;
    }

    uint32_t msg_cnt;
    uint32_t cons_cnt;
    if(pChannel->declareQueue("hello-queue", msg_cnt, cons_cnt, false/*passive*/, true/*durable*/, false/*exclusive*/, false/*auto_delete*/) < 0){
        std::cout << "Declare Queue Failed!" << std::endl;
        return false;
    }
    std::cout << ":" << msg_cnt << ", " << cons_cnt << std::endl;

    if (pChannel->bindQueue("hello-queue", "hello-exchange", "*")) {
        std::cout << "bindExchange Error!" << std::endl;
        return false;
    }

    if (pChannel->basicQos(1, true) < 0) {
        std::cout << "basicQos Failed!" << std::endl;
        return false;
    }

    if (pChannel->basicConsume("hello-queue", "*", false/*no_local*/, false/*no_ack*/, false/*exclusive*/) < 0) {
        std::cout << "BasicConosume Failed!" << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {

    int keyMask = 0;
    if (argc < 2) {
        printf("./a.out keyMask");
        return -1;
    }

    keyMask = ::atoi(argv[1]);

    AMQP::RabbitMQHelper mq("amqp://tibank:1234@127.0.0.1:5672/tibank_host");
    if (mq.doConnect() < 0) {
        std::cout << "Connect Error!" << std::endl;
        return -1;
    }

	amqp_channel_t t = mq.createChannel();
	if (t <= 0) {
        std::cout << "Create channel failed!" << std::endl;
        return -1;
    }

    if (mq.setupChannel(t, setupChannel, NULL) < 0) {
        std::cout << "Setup channel failed!" << std::endl;
		mq.freeChannel(t);
        return -1;
    }

    AMQP::RabbitMessage rabbitMsg;
    while (true) {
        if(mq.basicConsumeMessage(rabbitMsg, NULL, 0) < 0) {
retry_1:
            if (!mq.isConnectionOpen()) {
                if (mq.doConnect() < 0) {
                    std::cout << "Connect Error!" << std::endl;
                    ::sleep(1);
                    goto retry_1;
                }
            }
retry_2:
            if (!mq.isChannelOpen(t)) {
				mq.freeChannel(t);
				t = mq.createChannel();
                if (t <= 0) {
                    std::cout << "Create channel failed!" << std::endl;
                    ::sleep(1);
                    goto retry_2;
                }

                if (mq.setupChannel(t, setupChannel, NULL) < 0) {
					mq.freeChannel(t);
                    std::cout << "Setup channel failed!" << std::endl;
                    ::sleep(1);
                    goto retry_2;
                }
            }
        }

        std::string str = std::string((const char*)(rabbitMsg.content().bytes), rabbitMsg.content().len);
        std::string subStr = str.substr(str.find_first_of(":")+1);
        std::cout << subStr << std::endl;
        uint64_t nValue = ::atoll(subStr.c_str());

        if (nValue % keyMask == 0) {
            mq.basicAck(t, rabbitMsg.envelope.delivery_tag);
            std::cout << "ACK:" << rabbitMsg.envelope.delivery_tag << ", value:" << str << std::endl;
        } else {
            mq.basicReject(t, rabbitMsg.envelope.delivery_tag, true);
            std::cout << "Reject:" << rabbitMsg.envelope.delivery_tag << ", value:" << str << std::endl;
        }
        rabbitMsg.safe_clear();
    }

    return 0;
}
