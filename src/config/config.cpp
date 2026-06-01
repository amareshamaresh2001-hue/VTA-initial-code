#include "config.h"

std::string module_type(std::string& instName){
    if (instName == "FINISH"){return "COMPUTE";}
    else if (instName == "LOAD UOP"){return "COMPUTE";}
    else if (instName == "GEMM"){return "COMPUTE";}       
    else if (instName == "LOAD ACC"){return "COMPUTE";}  
    else if (instName == "ALU - add"){return "COMPUTE";} 
    else if (instName == "ALU - max imm"){return "COMPUTE";} 
    else if (instName == "ALU - add imm"){return "COMPUTE";} 
    else if (instName == "ALU - shr"){return "COMPUTE";} 
    else if (instName == "ALU - min imm"){return "COMPUTE";} 
    else if (instName == "NOP-COMPUTE-STAGE"){return "COMPUTE";}

    else if (instName == "LOAD INP"){return "LOAD";}
    else if (instName == "LOAD WGT"){return "LOAD";} 
    else if (instName == "NOP-MEMORY-STAGE"){return "LOAD";}  

    else if (instName == "NOP-STORE-STAGE"){return "STORE";}  
    else if (instName == "STORE"){return "STORE";} 

    else {return "Undefined";}  
}
