/**
 *
 * @file interrupts_101182048_101297742.cpp
 * @author Sasisekhar Govind
 *
 */


#include "interrupts_101182048_101297742.hpp"

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string trace;      //!< string to store single line of trace file
    std::string execution = "";  //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;

    //parse each line of the input trace file. 'for' loop to keep track of indices.
    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") { //As per Assignment 1
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } else if(activity == "SYSCALL") { //As per Assignment 1
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR (ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR(ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr)
           + ", cloning the PCB\n";
            current_time += duration_intr;

            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
            

            static int next_pid = 1;    //static bc it remains unchanged eveery time this function is run             
            PCB parent = current;          
            parent.state = "waiting";

            wait_queue.push_back(parent);  

            PCB child = current;                    
            child.PID  = next_pid++;
            child.PPID = parent.PID;
            child.state = "running";

            child.size = parent.size;               

            // try to allocate a free partition for the child
            if (!allocate_memory(&child)) {
                execution += std::to_string(current_time) + ", 0, FORK error: no partition for child\n";
                // (you can choose to bail or continue; the sample assumes it succeeds)
            }

            current = child;
            //adding current program to system status
            system_status += "time: " + std::to_string(current_time) + "; current trace: FORK, " + std::to_string(duration_intr) + "\n";
            system_status += "PID: " + std::to_string(current.PID)
               + " | program name: " + current.program_name
               + " | partition: " + std::to_string(current.partition_number)
               + " | size: " + std::to_string(current.size)
               + " | state: " + current.state + "\n";   

            //adding all waiting programs to system status
            for (int k = 0; k < wait_queue.size(); k++) {
                    PCB p = wait_queue[k];
                    system_status += "PID: " + std::to_string(p.PID)
                            + " | program name: " + p.program_name
                            + " | partition: " + std::to_string(p.partition_number)
                            + " | size: " + std::to_string(p.size)
                            + " | state: " + p.state + "\n";
                }
            system_status += "\n";

            //The following loop helps you do 2 things:
            // * Collect the trace of the child (and only the child, skip parent)
            // * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") {
                    skip = false;
                    continue;
                } else if(_activity == "IF_PARENT"){
                    skip = true;
                    parent_index = j;
                    if(exec_flag) {
                        break;
                    }
                } else if(skip && _activity == "ENDIF") {
                    skip = false;
                    continue;
                } else if(!skip && _activity == "EXEC") {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                }

                if(!skip) {
                    child_trace.push_back(trace_file[j]);
                }
            }
            i = parent_index;

    
            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the child's trace, run the child (HINT: think recursion)
            {
            auto [child_exec, child_status, t_after] =
                simulate_trace(child_trace,
                            current_time,          
                            vectors, delays, external_files,
                            current,                
                            wait_queue);            

            execution     += child_exec;               
            system_status += child_status;             
            current_time   = t_after;                 
            }
            //restoring parent
           for (int j = 0; j < wait_queue.size(); j++) {
                if (wait_queue[j].PID == parent.PID) {
                    wait_queue.erase(wait_queue.begin() + j);
                    break;
                }
            }

            current = parent;              
            current.state = "running";  


            ///////////////////////////////////////////////////////////////////////////////////////////


        } else if(activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your EXEC output here
                        // f) look up program size from external_files
            int new_size = get_size(program_name, external_files);
            if (new_size < 0) {
                execution += std::to_string(current_time) + ", 0, EXEC error: program not found (" + program_name + ")\n";
            } else {
                // Snapshot old mapping
                int old_partition = current.partition_number;

                current.program_name = program_name;
                current.size = new_size;

                bool reuse_same = memory[old_partition - 1].size >= (unsigned)current.size;
                if (reuse_same) {
                    memory[old_partition - 1].code = current.program_name;
                } else {
                    // free old and allocate a new one
                    PCB before = current;
                    before.partition_number = old_partition;
                    free_memory(&before);

                    if (!allocate_memory(&current)) {
                        execution += std::to_string(current_time) + ", 0, EXEC error: no suitable partition\n";
                    }
                }

                // === Match the sample output wording/order ===
                // "Program is N Mb large"
                        
                execution += std::to_string(current_time) + ", " + std::to_string(duration_intr)
                 + ", Program is " + std::to_string(current.size) + " Mb large\n";
                current_time += duration_intr;

                // "loading program into memory  //NMb * 15ms/Mb = Tms"
                int loader_ms = current.size * 15;   // adjust if your handout says otherwise
                execution += std::to_string(current_time) + ", "
                        + std::to_string(loader_ms)
                        + ", loading program into memory\n";
                current_time += loader_ms;

                // "marking partition as occupied"
                execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
                current_time += 3;

                // "updating PCB"
                execution += std::to_string(current_time) + ", 6, updating PCB\n";
                current_time += 6;

                execution += std::to_string(current_time) + ", 0, scheduler called\n";
                execution += std::to_string(current_time) + ", 1, IRET\n";
                current_time += 1;

                // === system_status snapshot (after IRET) ===
                system_status += "time: " + std::to_string(current_time)
                            +  "; current trace: EXEC " + current.program_name + ", " + std::to_string(duration_intr) + "\n";

                // running first
                system_status += "PID: " + std::to_string(current.PID)
                            +  " | program name: " + current.program_name
                            +  " | partition: " + std::to_string(current.partition_number)
                            +  " | size: " + std::to_string(current.size)
                            +  " | state: " + current.state + "\n";

                for (int j = 0; j < (int)wait_queue.size(); j++) {
                    const PCB& p = wait_queue[j];
                    system_status += "PID: " + std::to_string(p.PID)
                                +  " | program name: " + p.program_name
                                +  " | partition: " + std::to_string(p.partition_number)
                                +  " | size: " + std::to_string(p.size)
                                +  " | state: " + p.state + "\n";
                }
                system_status += "\n";
            }



            ///////////////////////////////////////////////////////////////////////////////////////////


            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)
            auto [exec_exec, exec_status, t_after] =
            simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);

            execution     += exec_exec;
            system_status += exec_status;
            current_time   = t_after;

            ///////////////////////////////////////////////////////////////////////////////////////////

            break; //Why is this important? (answer in report)

        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {

    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elemens is the device number, starting from 0
    //external_files is a C++ std::vector of the struct 'external_file'. Check the struct in 
    //interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    //Just a sanity check to know what files you have
    print_external_files(external_files);

    //Make initial PCB (notice how partition is not assigned yet)
    PCB current(0, -1, "init", 1, 0, "waiting", 0, 0 , 0);
    //Update memory (partition is assigned here, you must implement this function)
    if(!allocate_memory(&current)) {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;

  
    //Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(   trace_file, 
                                            0, 
                                            vectors, 
                                            delays,
                                            external_files, 
                                            current, 
                                            wait_queue);

    input_file.close();

    write_output(execution, "execution4.txt");
    write_output(system_status, "system_status4.txt");

    return 0;
}
