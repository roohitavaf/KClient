#include <iostream>

#include <boost/filesystem.hpp>
#include <librdkafka/rdkafkacpp.h>
#include <set>
#include <map>
#include <thread>
#include <mutex>
#include <sstream>
#include "kclient.h"



size_t produce_file(const std::string& fname, KProducer& producer, KTopic& topic, int32_t part)
{
	std::ifstream f{fname};

	size_t n_msg{};
	while (f)
	{
		std::string line;
		std::getline(f, line);
		if (!topic.sync_produce(line, part))
			break; //error
		n_msg++;
	}
	return n_msg;
}

size_t produce_file(const std::string& fname, std::ofstream& fout)
{
	size_t n_msg{};
	std::ifstream f{fname};

	while(f)
	{
		std::string line;
		std::getline(f, line);
		fout << line << "\n";
		n_msg++;
	}
	fout.flush();
	return n_msg;
}


void zmq_server();
void zmq_client();


void fake_producer(const std::map<std::string, std::string>& params)
{
	using namespace boost::filesystem;

	try
	{
		size_t n_msg{};
		std::ofstream f_out{params.at("data_out") + "/test_out.txt"};

		path p(params.at("data_in"));
		for (directory_entry& x : directory_iterator(p))
		{
			const auto fname = x.path().string();
			n_msg += produce_file(x.path().string(), f_out);
			std::cout << "done: " << fname << "\n";
		}

		std::cout << "Tot message: " << n_msg << "\n";
	}
	catch (std::exception& ex)
	{
		std::cerr << "Error: " << ex.what() << std::endl;
	}
}


void producer(KClient& client, const std::map<std::string, std::string>& params)
{
	using namespace boost::filesystem;
	client.setTopicConf("auto.commit.enable", "true");

	try
	{
		size_t n_msg{};
		int32_t part{RdKafka::Topic::PARTITION_UA};
		//std::ofstream f_out{"test_out.txt"};
		KProducer producer = client.create_producer();
		std::cout << "> Created producer " << producer.name() << std::endl;

		// Create topic handle.
		KTopic topic = producer.create_topic(params.at("topic"));
		if (params.find("partition") != params.end())
			part = std::stoi(params.at("partition"));

		//
		path p(params.at("data_in"));
		for (directory_entry& x : directory_iterator(p))
		{
			const auto fname = x.path().string();
			n_msg += produce_file(fname, producer, topic, part);
			//produce_file(x.path().string(), f_out);
			std::cout << "done: " << fname << "\n";
			producer.poll(10);
		}

		while (producer.outq_len() > 0)
		{
			std::cout << "Waiting for " << producer.outq_len() << std::endl;
			producer.poll(100);
		}

		std::cout << "Tot message: " << n_msg << "\n";
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

	client.setTopicConf("auto.commit.enable", "true");
	client.setTopicConf("auto.offset.reset", "latest");

	try
	{
		KConsumer consumer = client.create_consumer();
		std::cout << "> Created consumer " << consumer.name() << std::endl;

		consumer.subscribe({params.at("topic")});
		consumer.wait_rebalance();
		size_t msg_cnt{};

		consumer.for_each(1000, [&msg_cnt](const RdKafka::Message& message){
			//std::cout << "Read msg at offset " << message->offset() << "\n";
			if (message.key())
				std::cout << "Key: " << message.key() << "\n";

			if (message.payload() == nullptr)
				return;
			//std::cout << static_cast<const char *>(message->payload()) << "\n";
			msg_cnt++;
			if (msg_cnt % 1000 == 0)
			{
				std::cout << "*";
				std::flush(std::cout);
			}
		}, [&consumer, &params, &msg_cnt](const RdKafka::Message& message, const RdKafka::ErrorCode err_code){
			if (err_code != RdKafka::ERR__PARTITION_EOF)
			{
				std::cerr << "Error consuming message!\n";
				return false;
			}
			consumer.reset_eof_partion();
			consumer.commit();
			std::cout << "\nEnd: " << msg_cnt << "\n";
			msg_cnt = 0;
			return params.find("exit") != params.end();
		});

		consumer.close();
	}
	catch (std::exception& ex)
	{
		std::cerr << "Error: " << ex.what() << std::endl;
	}
}


void setup_kclient(KClient& client, std::map<std::string, std::string>& params)
{
	/*
	 * Set basic configuration
	 */
	if (!client.setGlobalConf("statistics.interval.ms", "5000"))
		exit(1);

	if (!params["compression"].empty() && !client.setGlobalConf("compression.codec", params.at("compression")))
		exit(1);

	if (!client.setGlobalConf("client.id", params["client.id"]))
		exit(1);

	// load metadata
	if (params["metadata"] == "true" && !client.loadMetadata(params["topic"]))
	{
		std::cerr << "Problem loading metadata\n";
		exit(1);
	}
}



int main(int argc, char* argv[])
{
	enum mode_t {PRODUCER, CONUSMER, ZEROMQ_SERVER, ZEROMQ_CLIENT, FAKE_PRODUCER};

	char hostname[128];
	if (gethostname(hostname, sizeof(hostname)))
	{
		std::cerr << "Failed to lookup hostname\n";
		exit(1);
	}

	std::map<std::string, std::string> params;
	params["brokers"] = "localhost";
	params["topic"] = "test";
	params["client.id"] = hostname;
	params["data_out"] = ".";
	params["metadata"] = "";

	/*
	 * Read input parameter
	 */

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
		}
		else if(strcmp(argv[i], "-p") == 0)
		{
			params["partition"] = argv[i+1];
			i++;
		}
		else if(strcmp(argv[i], "--load-metadata") == 0)
		{
			params["metadata"] = "true";
		}
		else if(strcmp(argv[i], "--data-in") == 0)
		{
			params["data_in"] = argv[i+1];
			i++;
		}
		else if(strcmp(argv[i], "--data-out") == 0)
		{
			params["data_out"] = argv[i+1];
			i++;
		}
		else if(strcmp(argv[i], "-e") == 0)
		{
			params["exit"] = "true";
		}
	}


	mode_t mode;
	if (params["mode"] == "producer")
		mode = PRODUCER;
	else if (params["mode"] == "consumer")
		mode = CONUSMER;
	else if (params["mode"] == "fake-producer")
		mode = FAKE_PRODUCER;
	else if (params["mode"] == "zmq-server")
		mode = ZEROMQ_SERVER;
	else if (params["mode"] == "zmq-client")
		mode = ZEROMQ_CLIENT;
	else
	{
		std::cerr << "unk mode!\n";
		exit(1);
	}

	switch (mode)
	{
		case PRODUCER:
		{
			KClient client{params["brokers"]};
			setup_kclient(client, params);

			producer(client, params);
			RdKafka::wait_destroyed(5000);
			break;
		}
		case CONUSMER:
		{
			KClient client{params["brokers"]};
			setup_kclient(client, params);
			consumer(client, params);
			RdKafka::wait_destroyed(5000);
			break;
		}
		case FAKE_PRODUCER:
		{
			fake_producer(params);
			break;
		}
		case ZEROMQ_SERVER:
		{
			zmq_server();
			break;
		}
		case ZEROMQ_CLIENT:
		{
			zmq_client();
			break;
		}
	}

	return 0;
}
