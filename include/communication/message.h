#pragma once

#include <string>
#include "json.hpp"

using json = nlohmann::json;

namespace message_types {
    class message 
    {
        public:
        message() = delete;
        message(const message&) = default;
        message& operator=(const message&) = default;
        message(message&&) = default;
        message& operator=(message&&) = default;
        ~message() = default;

        message(const std::string& sender):
            m_sender{sender},
            m_msg_queue_name{"msg_queue"}
            {
                m_message_content["sender"] = sender;
            }

        template<typename T>
        message(const std::string& sender, const std::string& receiver, const std::string& type, const T& data):
            m_sender{sender},
            m_msg_queue_name{"msg_queue"}
            {
                m_message_content["sender"] = sender;
                m_message_content["receiver"] = receiver;
                m_message_content["type"] = type;
                m_message_content["data"] = data;
            }
                
        json create_err_msg(const std::string& receiver, const std::string& data  = "Invalid message"){
            m_message_content["receiver"] = receiver;
            m_message_content["type"] = "Error";
            m_message_content["data"] = data;
            return m_message_content;
        }
        template<typename T>
        json create_req_msg(const std::string& receiver, const T& data){
            m_message_content["receiver"] = receiver;
            m_message_content["type"] = "Request";
            m_message_content["data"] = data;
            return m_message_content;
        }
        template<typename T>
        json create_schedule_msg(const std::string& port, const T& data) {
            m_message_content["receiver"] = port;
            m_message_content["type"] = "Response";
            m_message_content["data"] = data;
            return m_message_content;
        }
        template<typename T>
        json create_res_msg(const std::string& receiver, const T& data) {
            m_message_content["receiver"] = receiver;
            m_message_content["type"] = "Response";
            m_message_content["data"] = data;
            return m_message_content;
        }

        template<typename T>
        // echo back the message
        json create_ack_msg(const std::string& receiver, const T& data){
            m_message_content["receiver"] = receiver;
            m_message_content["type"] = "Ack";
            m_message_content["data"] = data;
            return m_message_content;
        }

        json create_empty_msg(const std::string& receiver){
            json msg_data;
            msg_data["content"] = "Empty";
            
            m_message_content["receiver"] = receiver;
            m_message_content["type"] = "Empty";
            m_message_content["data"] = msg_data;
            
            return m_message_content;
        }
        // this is used in message queue and should be transparent 
        // to the sender and receiver, wrapper puts actual msg as data
        template<typename T>
        json create_enq_req(const std::string& task_id, const std::string& port_name, const std::string& queue_name, const T& data){
            json msg_data;
            msg_data["task_id"] = task_id;
            msg_data["port_name"] = port_name;
            msg_data["number"] = 1;
            msg_data["type"] = "write";
            msg_data["queue"] = queue_name;
            msg_data["data"] = data;
            

            m_message_content["receiver"] = m_msg_queue_name;
            m_message_content["type"] = "Request";
            m_message_content["data"] = msg_data;
            return m_message_content;
        }

        json create_deq_req(const std::string& task_id, const std::string& port_name, const std::string& queue_name, int16_t number = 1) {
            json msg_data;
            msg_data["task_id"] = task_id;
            msg_data["port_name"] = port_name;
            msg_data["type"] = "read";
            msg_data["number"] = number;
            msg_data["queue"] = queue_name;

            m_message_content["receiver"] = m_msg_queue_name;
            m_message_content["type"] = "Request";
            m_message_content["data"] = msg_data;
            return m_message_content;
        }
        json create_read_sample_req (const std::string& task_id, const std::string& port_name, const std::string& msg_queue){
            json msg_data;
            msg_data["task_id"] = task_id;
            msg_data["port_name"] = port_name;
            msg_data["type"] = "read_sample";
            msg_data["queue"]= msg_queue;

            m_message_content["receiver"] = m_msg_queue_name;
            m_message_content["type"] = "Request";
            m_message_content["data"] = msg_data;
            return m_message_content;
        }

        json create_quelen_req (const std::string& sender, const std::string& receiver, const std::string& msg_queue){
            json msg_data;
            msg_data["task_id"] = sender;
            msg_data["port_name"] = receiver;
            msg_data["type"] = "get_len";
            msg_data["queue"]= msg_queue;

            m_message_content["receiver"] = m_msg_queue_name;
            m_message_content["type"] = "Request";
            m_message_content["data"] = msg_data;
            return m_message_content;
        }
        template<typename T>
        json write_schedule(const std::string& msg_id, const std::string& port_name, const std::string& queue_name, const T& data) {
            json msg_data;
            msg_data["task_id"] = msg_id;
            msg_data["port_name"] = port_name;
            msg_data["number"] = 1;
            msg_data["type"] = "write";
            msg_data["queue"] = queue_name;
            msg_data["data"] = data;

            m_message_content["receiver"] = m_msg_queue_name;
            m_message_content["type"] = "Request";
            m_message_content["data"] = msg_data;
            return m_message_content;

        }
        json read_schedule(const std::string& task_id, const std::string& port_name, const std::string& queue_name, int16_t number = 1) {
            json msg_data;
            msg_data["task_id"] = task_id;
            msg_data["port_name"] = port_name;
            msg_data["type"] = "read";
            msg_data["number"] = number;
            msg_data["queue"] = queue_name;

            m_message_content["receiver"] = m_msg_queue_name;
            m_message_content["type"] = "Request";
            m_message_content["data"] = msg_data;
            return m_message_content;

        }
        private:
        std::string m_sender;
        std::string m_msg_queue_name;
        json m_message_content;
        
    };
}

