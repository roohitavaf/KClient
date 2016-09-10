#include <iostream>

#include <unistd.h>
#include <librdkafka/rdkafkacpp.h>
#include <functional>
#include <memory>
#include <set>
#include <map>
#include <thread>
#include <mutex>
#include <cstring>
#include "util.h"
#include "kclient.h"



void producer(KClient& client, const std::map<std::string, std::string>& params)
{
    try
    {
        KProducer producer = client.create_producer();
        std::cout << "> Created producer " << producer.name() << std::endl;

        // Create topic handle.
        KTopic topic = producer.create_topic(params.at("topic"));

        // Produce some message
        auto p_it = topic.getPartions().begin();
        for (size_t i = 0; i < 1000000; i++)
        {
            if (p_it == topic.getPartions().end())
                p_it = topic.getPartions().begin();

            RdKafka::ErrorCode resp = topic.produce("Hello World! " + std::to_string(i), *p_it);

            if (resp != RdKafka::ERR_NO_ERROR)
            {
                std::cerr << "> Produce failed: " << RdKafka::err2str(resp) << std::endl;
                break;
            }
            ++p_it;
        }

        while (producer.outq_len() > 0)
        {
            std::cout << "Waiting for " << producer.outq_len() << std::endl;
            producer.poll(1000);
        }
    }
    catch (std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
    }
}


void consumer(KClient& client, const std::map<std::string, std::string>& params)
{
    if (params.find("group.id") != params.end())
        client.setGlobalConf("group.id", params.at("group.id"));

    bool exit_end{false};
    if (params.find("exit_end") != params.end() && params.at("exit_end") == "true")
        exit_end = true;

    try
    {
        KConsumer consumer = client.create_consumer();
        std::cout << "> Created consumer " << consumer.name() << std::endl;

        KQueue queue = consumer.create_queue();
        queue.for_each(1000, [](RdKafka::Message& message){
            std::cout << "Read msg at offset " << message.offset() << "\n";
            if (message.key())
                std::cout << "Key: " << *message.key() << "\n";

            std::cout << static_cast<const char *>(message.payload()) << "\n";
        }, [](RdKafka::Message& message){
            std::cerr << "Error reading message or EOF\n";
        }, exit_end);
    }
    catch (std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
    }
}


int main(int argc, char* argv[])
{
    enum mode_t {PRODUCER, CONUSMER};

    char hostname[128];
    if (gethostname(hostname, sizeof(hostname)))
    {
        std::cerr << "Failed to lookup hostname\n";
        exit(1);
    }

    /*
     * Read input parameter
     */

    std::map<std::string, std::string> params;
    for (int i = 0; i < argc; i++)
    {
        if(strcmp(argv[i], "--topic") == 0)
        {
            params["topic"] = argv[i+1];
            i++;
        }
        else if(strcmp(argv[i], "--group-id") == 0)
        {
            params["group.id"] = argv[i+1];
            i++;
        }
        else if(strcmp(argv[i], "--client-id") == 0)
        {
            params["client.id"] = argv[i+1];
            i++;
        }
        else if(strcmp(argv[i], "--mode") == 0)
        {
            params["mode"] = argv[i+1];
            i++;
        }
        else if(strcmp(argv[i], "--brokers") == 0)
        {
            params["brokers"] = argv[i+1];
            i++;
        }
        else if(strcmp(argv[i], "-z") == 0)
        {
            params["compression"] = "snappy";
            i++;
        }
    }

    if (params["brokers"].empty())
        params["brokers"] = "localhost";

    if (params["topic"].empty())
        params["topic"] = "test";

    if (params["client.id"].empty())
        params["client.id"] = hostname;


    KClient client(params["brokers"]);
    std::string topic_str{params["topic"]};
    mode_t mode{PRODUCER};

    if (params["mode"] == "producer")
        mode = PRODUCER;
    else if (params["mode"] == "consumer")
        mode = CONUSMER;
    else
    {
        std::cerr << "unk mode!\n";
        exit(1);
    }


    /*
     * Set basic configuration
     */

    if (!client.setGlobalConf("statistics.interval.ms", "5000"))
        exit(1);

    if (!params["compression"].empty() && !client.setGlobalConf("compression.codec", "snappy"))
        exit(1);

    if (!client.setGlobalConf("client.id", params["client.id"]))
        exit(1);


    // load metadata
    if (!client.loadMetadata())
    {
        std::cerr << "Problem loading metadata\n";
        exit(1);
    }

    switch (mode)
    {
        case PRODUCER:
            producer(client, params);
            break;
        case CONUSMER:
            consumer(client, params);
            break;
    }

    RdKafka::wait_destroyed(5000);

    return 0;
}