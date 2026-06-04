#include <iostream>
#include <fstream>
#include <thread>

#include "communication.h"
#include "message.h"

com_socket_zmq::com_socket_zmq(zmq::context_t &context, const std::string &addr, const std::string &type)
{
    try
    {
        if (type == "bind")
        {
            socket = zmq::socket_t(context, zmq::socket_type::rep);
            socket.bind(addr);
        }
        else if (type == "connect")
        {
            socket = zmq::socket_t(context, zmq::socket_type::req);
            socket.connect(addr);
        }
    }
    catch (zmq::error_t &e)
    {
        std::cerr << "Zmq Socket Error: " << e.what() << std::endl;
    }
};

com_socket_zmq::~com_socket_zmq()
{
    socket.close();
};

com_socket_zmq &com_socket_zmq::operator=(com_socket_zmq &&other) noexcept
{
    if (this != &other)
    {
        socket = std::move(other.socket);
    }
    return *this;
}

void com_socket_zmq::send(std::string message)
{
    zmq::message_t reply(message.begin(), message.end());
    socket.send(reply, zmq::send_flags::none);
};

std::string com_socket_zmq::recv(std::string sender_name)
{
    zmq::message_t request;
    auto msg = socket.recv(request, zmq::recv_flags::none);
    return request.to_string();
};

com_socket_queue::com_socket_queue(std::string component_name) : m_component_name{component_name}
{

    std::string config_file = "system_config.json";
    nlohmann::json sys_config;
    nlohmann::json msg_schema;

    std::ifstream file_stream(config_file);
    if (!file_stream.is_open())
    {
        std::cout << "Error opening file: " << config_file << std::endl;
        throw std::runtime_error("Error opening file: " + config_file);
    }
    file_stream >> sys_config;
    file_stream.close();
    std::string msg_schema_file = sys_config["object_schema"]["msg_sh"];

    file_stream.open(msg_schema_file);
    if (!file_stream.is_open())
    {
        std::cerr << "Error opening file: " << msg_schema_file << std::endl;
        throw std::runtime_error("Error opening file: " + msg_schema_file);
    }
    file_stream >> msg_schema;

    file_stream.close();
    m_validator_msg.set_root_schema(msg_schema);

    std::string send_addr = sys_config["msg_queue"]["enqueue"]["addr"];
    std::string recv_addr = sys_config["msg_queue"]["dequeue"]["addr"];

    m_socket_dequeue = zmq::socket_t(m_context, zmq::socket_type::req);
    m_socket_enqueue = zmq::socket_t(m_context, zmq::socket_type::req);
    m_socket_dequeue.connect(recv_addr);
    m_socket_enqueue.connect(send_addr);
}

com_socket_queue::~com_socket_queue()
{
    m_socket_dequeue.close();
    m_socket_enqueue.close();
};

// allow move assignment
com_socket_queue &com_socket_queue::operator=(com_socket_queue &&other) noexcept
{
    if (this != &other)
    {
        m_context = std::move(other.m_context);
        m_socket_dequeue = std::move(other.m_socket_dequeue);
        m_socket_enqueue = std::move(other.m_socket_enqueue);
        m_component_name = std::move(other.m_component_name);
        m_validator_msg = std::move(other.m_validator_msg);
    }
    return *this;
}

bool com_socket_queue::send_data(const nlohmann::json &message)
{

    // log the message to central
    std::cout << "sending message " << message.dump() << std::endl;

    bool valid_msg = validate_msg(message);

    if (!valid_msg)
    {
        return false;
    }

    message_types::message enqueue_req(m_component_name);

    std::string receiver = message["receiver"]; // port number
    std::string req_msg = enqueue_req.create_enq_req(receiver, receiver, "sender", message).dump();

    std::cout << "sending request to msg_queue " << req_msg << std::endl;
    // Simply forwards the message to the message queue instead of sending it to the receiver
    zmq::message_t request(req_msg.begin(), req_msg.end());
    m_socket_enqueue.send(request, zmq::send_flags::none);

    // Get confirmation from the message queue that the message has been received
    zmq::message_t reply;

    auto msg = m_socket_enqueue.recv(reply, zmq::recv_flags::none);
    // valid_msg = validate_msg(reply.to_string());
    valid_msg = true;

    if (!valid_msg)
    {
        return false;
    }
    std::cout << "reply from msg_queue " << reply.to_string() << std::endl;

    return true;
}

bool com_socket_queue::send_schedule(const nlohmann::json &message)
{

    // log the message to central
    std::cout << "sending message " << message.dump() << std::endl;

    bool valid_msg = validate_msg(message);

    if (!valid_msg)
    {
        return false;
    }

    message_types::message enqueue_req(m_component_name);

    std::string port = message["receiver"]; // port number

    // std::string req_msg = enqueue_req.create_enq_req(m_component_name, receiver, "sender", message).dump();
    std::cout << " component name = " << m_component_name << std::endl;
    std::cout << " receiver = " << port << std::endl;
    std::string req_msg = enqueue_req.write_schedule(m_component_name, "1000", "sender", message).dump();
    // std::string req_msg = enqueue_req.write_schedule(m_component_name, receiver, "sender", message).dump();
    std::cout << "sending request to msg_queue " << req_msg << std::endl;
    // Simply forwards the message to the message queue instead of sending it to the receiver
    zmq::message_t request(req_msg.begin(), req_msg.end());
    m_socket_enqueue.send(request, zmq::send_flags::none);

    // Get confirmation from the message queue that the message has been received
    zmq::message_t reply;

    auto msg = m_socket_enqueue.recv(reply, zmq::recv_flags::none);
    // valid_msg = validate_msg(reply.to_string());
    valid_msg = true;

    if (!valid_msg)
    {
        return false;
    }
    std::cout << "reply from msg_queue " << reply.to_string() << std::endl;

    return true;
}

nlohmann::json com_socket_queue::recv_data(const std::string &sender, int number)
{
    // if msg queue is empty, send error message
    // receive message from the queue, block until the message is received
    nlohmann::json reply;
    std::string reply_status{"Empty"};
    message_types::message dequeue_req(m_component_name);

    // std::string request= dequeue_req.create_deq_req(sender, m_component_name, "receiver", number).dump();
    std::string request = dequeue_req.create_deq_req(sender, sender, "sender", number).dump();

    std::cout << "sending request to msg_queue " << request << std::endl;

    zmq::message_t request_msg{request.begin(), request.end()};
    m_socket_dequeue.send(request_msg, zmq::send_flags::none);

    zmq::message_t reply_msg;
    auto msg = m_socket_dequeue.recv(reply_msg, zmq::recv_flags::none);

    std::cout << "reply from msg_queue " << reply_msg.to_string() << std::endl;

    reply = nlohmann::json::parse(reply_msg.to_string());
    bool valid_msg = validate_msg(reply);

    std::cout << "validation status of reponse msg" << valid_msg << std::endl;

    reply_status = reply["type"];
    // block until msg_queue returns a valid message
    /*  while(reply_status == "Empty") {
              zmq::message_t request_msg{request.begin(), request.end()};
              m_socket_dequeue.send(request_msg, zmq::send_flags::none);

              zmq::message_t reply_msg;
              m_socket_dequeue.recv(reply_msg, zmq::recv_flags::none);

              std::cout << "reply from msg_queue " << reply_msg.to_string() << std::endl;

              reply = nlohmann::json::parse(reply_msg.to_string());
              bool valid_msg = validate_msg(reply);

              std::cout << "validation status of reponse msg" << valid_msg << std::endl;

              reply_status = reply["type"];
              std::this_thread::sleep_for(std::chrono::milliseconds(1000));
          }*/
    return reply;
}

nlohmann::json com_socket_queue::read_schedule(const std::string &ProcessingElement, int number)
{
    // if msg queue is empty, send error message
    // receive message from the queue, block until the message is received
    nlohmann::json reply;
    std::string reply_status{"Empty"};
    message_types::message dequeue_req(m_component_name);

    // std::string request = dequeue_req.create_deq_req(sender, m_component_name, "receiver", number).dump();
    std::string request = dequeue_req.read_schedule(ProcessingElement, "1000", "queue", number).dump();
    std::cout << "sending request to msg_queue " << request << std::endl;

    // block until msg_queue returns a valid message
    while (reply_status == "Empty")
    {
        zmq::message_t request_msg{request.begin(), request.end()};
        m_socket_dequeue.send(request_msg, zmq::send_flags::none);

        zmq::message_t reply_msg;
        auto msg = m_socket_dequeue.recv(reply_msg, zmq::recv_flags::none);

        std::cout << "reply from msg_queue " << reply_msg.to_string() << std::endl;

        reply = nlohmann::json::parse(reply_msg.to_string());
        bool valid_msg = validate_msg(reply);

        std::cout << "validation status of reponse msg" << valid_msg << std::endl;

        reply_status = reply["type"];
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    return reply;
}

bool com_socket_queue::validate_msg(const nlohmann::json &msg)
{
    try
    {
        m_validator_msg.validate(msg);
    }
    catch (std::exception &e)
    {
        std::cerr << "invalid message " << msg.dump() << e.what() << std::endl;
        return false;
    }
    return true;
}

bool com_socket_queue::validate_msg(const std::string &msg)
{
    try
    {
        json msg_json = json::parse(msg);
        m_validator_msg.validate(msg_json);
    }
    catch (std::exception &e)
    {
        std::cerr << "invalid message " << msg << e.what() << std::endl;
        return false;
    }
    return true;
}
