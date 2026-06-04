#pragma once

#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include "json-schema.hpp"
#include <string>

// zmq socket wrapper class
class com_socket_zmq {
    public:
    com_socket_zmq() = default;

    com_socket_zmq(zmq::context_t& context,  const std::string& addr, const std::string& type);
    ~com_socket_zmq();

    // copy not allowed
    com_socket_zmq(const com_socket_zmq& other) = delete;
    com_socket_zmq& operator=(const com_socket_zmq& other) = delete;

    // allow move assignment
    com_socket_zmq& operator=(com_socket_zmq&& other) noexcept;
    
    void send(std::string message);
    std::string recv(std::string sender_name ="ANY");
   // nlohmann::json_schema::json_validator m_validator_msg;

    private:
    zmq::context_t m_context{1};
    zmq::socket_t socket;

};

// zmq socket wrapper class for commmunication via message queue and middleware that reads 
// from sender queue and writes to receiver queue, these sockets don't need matching send/recv
class com_socket_queue 
{
public:
    com_socket_queue() = default;
    com_socket_queue( std::string component_name);
    ~com_socket_queue();

    // copy not allowed
    com_socket_queue(const com_socket_queue& other) = delete;
    com_socket_queue& operator=(const com_socket_queue& other) = delete;

    // allow move assignment
    com_socket_queue& operator=(com_socket_queue&& other) noexcept;

     
    bool send_data(const nlohmann::json& message);
    bool send_schedule(const nlohmann::json& message);
    //bool send(py::json msg);
    nlohmann::json recv_data(const std::string& sender ="ANY", int number = 1);
    nlohmann::json read_schedule(const std::string& sender = "ANY", int number = 1);

    private:
    bool validate_msg(const nlohmann::json& msg);
    bool validate_msg(const std::string& msg);

    std::string m_component_name{"NONE"};
    // msg schema validator as defined in ./system_config.json
    nlohmann::json_schema::json_validator m_validator_msg;
    
    // socket for send and recv requests to/from message queue
    
    zmq::context_t m_context{1};
    zmq::socket_t  m_socket_dequeue;
    zmq::socket_t  m_socket_enqueue;
    

};