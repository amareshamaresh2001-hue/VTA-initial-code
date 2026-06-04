#ifndef MODULE_H
#define MODULE_H

#include <systemc.h>
#include <vector>
#include <map>
#include <queue>

#include "fetcher.h"
#include "config.h"


class Module : public sc_module
{
public:

    sc_in<bool> i_queue_vld;
    sc_out<bool> i_queue_rdy;
    sc_in<sc_int<64>> i_queue_data;
    sc_in<bool> i_queue_end;

    Module(sc_core::sc_module_name nm);

    ~Module() {}

    SC_HAS_PROCESS(Module);

    void start();

protected:

    std::vector<sc_int<64>> current_instruction_part;
    Instruction* current{nullptr};
    
    sc_int<32> *result_data;
    
    bool waiting_i_queue_vld = false;
    bool i_queue_rdy_state = false;

    sc_event fetch;
    sc_event check_dep;
    sc_event receive_dep;
    sc_event dep_received;
    sc_event finish;
    sc_event push_dep;
    sc_event dep_pushed;

    sc_event activate_i_queue_rdy;
    sc_event read_i_queue_data;

    virtual sc_time latency() = 0;

    void fetch_instruction();

    virtual void check_dependencies() = 0;

    virtual void receive_dependencies() = 0;

    virtual void dependencies_received() = 0;

    virtual void finalize_instruction() = 0;

    virtual void push_dependencies() = 0;

    virtual void dependencies_pushed() = 0;

    // fetch instruction 
    void i_queue_vld_handler();
    void i_queue_end_handler();
    void activate_i_queue_rdy_handler();
    void read_i_queue_data_handler();

};







// class Module : public sc_module
// {
// public:
//     Module(sc_core::sc_module_name nm/*, char *schedules_dir, char *schedule_file_dir, std::vector<std::string> splits, std::map<std::string, std::vector<Instruction>> instructions*/);
//     ~Module(void);

//     SC_HAS_PROCESS(Module);

//     virtual void module_thread() = 0;

//     Instruction get_instruction_at(int val) const;
//     void print_num_instructions(std::string module_name) const;
//     int num_instructions();
//     void re_order() const;
//     void push_started_at_gtb(int id, long long val);
//     void push_completed_at_gtb(int id, long long val);
//     void module_clear();
//     void post_schedule_gen_proc();

//     void add_instruction_schedule(InstructionSechedule inst_schedule);
//     long long get_start_time(std::string split);
//     long long get_end_time(std::string split);
//     static std::vector<long long> compute_splits_durations(std::vector<Module*> load_modules, std::vector<Module*> compute_modules, std::vector<Module*> store_modules);

//     void save_dispatcher_schedule();
//     static void save_dispatchers_schedules(std::vector<Module*> load_modules, std::vector<Module*> compute_modules, std::vector<Module*> store_modules);
//     static void save_instructions_schedule(char * file_path, std::vector<std::string> splits, std::vector<Module*> load_modules, std::vector<Module*> compute_modules, std::vector<Module*> store_modules);
// protected:
//     char *schedules_dir;
//     char *schedule_file_dir;
//     char *schedule_file_path;

//     std::vector<std::string> splits;
//     int current_split_idx;
//     std::map<std::string, std::vector<Instruction>> instructions;
//     std::map<std::string, std::vector<InstructionSechedule>> instruction_schedules;

//     void *context;
//     void *socket;
    
//     inline static std::queue<long long> s2g_queue[4];
//     inline static std::queue<long long> g2s_queue[4];
//     inline static std::queue<long long> l2g_queue[4];
//     inline static std::queue<long long> g2l_queue[4];

//     void initialize_communication_stream(std::string port);
//     void close_communication_stream();
//     void send_message(char *message);
// };

#endif