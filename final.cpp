//
//  main.cpp
//  this program implements the tomasulo algorithm
//  instruction types supported: LD, SD, MULTD, DIVD, ADDD, SUBD
//  <store register>: [R0, Rn] where n is the number of registers (configured in assumptions)
//  Note: the program takes advantage of strncmp with 2 characters to find the correct register,
//  so if the number of registers is too large, the program may not execute correctly


#include <iostream>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXCHAR 1000

using namespace std;

// defining the data type for all instructions 
typedef struct instruction
{
    char type[100];
    char dest_reg[100];
    char reg_j[100];
    char reg_k[100];
    double num;
    double rs=0;
    double issue=-1;
    int start=0;
    double completion=-1;
    double written=-1;
    int result;
    int load=0;
    bool completing=false;
    bool writing=false;
} instruction;
// defining the data type for memory 
typedef struct memory
{
    char name[2];
    double data;
    int tag=0;
    bool busy=false;
} memory;
// defining the data type for all RS
typedef struct reservation_station
{
    char name[100]="NONE";
    double num;
    double data_j=NULL;
    double data_k=NULL;
    double result;
    int tag_j=0;
    int tag_k=0;
    int instr=-1;
    int cycle_count=0;
    int cycles_required=-999;
    bool executing=false;
    bool busy=false;

} reservation_station;

typedef struct load_store_rs
{
    char name[100]="NONE";
    double num;
    double address=NULL;
    int instr=-1;
    int tag=0;
    int cycle_count=0;
    int cycles_required=-999;
    bool executing=false;
    bool busy=false;
} load_store_rs;

void header(int n);
template<typename T> void printElement(T t, const int& width);
template<typename T> void printInstructionStatus(T t, const int& width);
template<typename T> void printLoadStatus(T t, const int& width);
template<typename T> void printStoreStatus(T t, const int& width);
template<typename T> void printStationStatus(T t, const int& width);
template<typename T> void printRegisterStatus(T t, const int& width);

int main() {
    // ==================== ASSUMPTIONS ====================
    // all assumptions are conssitent with the project description 
    int addCycles = 2;
    int subCycles = 2;
    int multCycles = 10;
    int diviCycles = 40;
    int loadCycles = 3;
    int storeCycles = 3;
    const int addReservationStations = 3;
    const int mulReservationStations = 2;
    const int loadReservationStations = 3;
    const int storeReservationStations = 3;
    const int maxInstructions = 16;
    const int numRegisters = 6;
    // initializing the values in registers, add or delete entries depending on the number of registers
    float dataRegisters[numRegisters] = {1, 2, 3, 4, 5, 6};
    //char* filename = "raw.txt";
    //char* filename = "waw.txt";
	char* filename = "war.txt";
    //char* filename = "sdld.txt";
    //char* filename = "stall.txt";
    //char* filename = "long.txt";
    
    // ==================== STRUCTURE INITIALIZATION ====================
    instruction instruction_list[maxInstructions];
    memory  data_registers[numRegisters];
    
    char rs_num[3];
    for (int i=0; i < numRegisters; i++){
        strcpy(data_registers[i].name, "R");
        sprintf(rs_num, "%i", i*2);
        strcat(data_registers[i].name, rs_num);
        data_registers[i].data = dataRegisters[i];
    }
    
    reservation_station add_rs[addReservationStations];
    reservation_station mul_rs[mulReservationStations];
    
    int j;
    for (j=0; j < addReservationStations; j++) {
        add_rs[j].num = j + 1;
    }
    int k;
    for (k=0; k < mulReservationStations; k++) {
        // each reservation station needs a unique number
        mul_rs[k].num = j + k + 1;
    }

    load_store_rs load_rs[loadReservationStations];
    load_store_rs store_rs[storeReservationStations];
    
    for (int i=0; i < loadReservationStations; i++) {
        load_rs[i].num = addReservationStations + mulReservationStations + i + 1;
    }
    
    for (int i=0; i < storeReservationStations; i++) {
        store_rs[i].num = addReservationStations + mulReservationStations + loadReservationStations + i + 1;
    }
    // 1,2,3 add rs;4,5 mul rs;6,7,8 load rs;9,10,11 store rs
    // ==================== VARIABLE INITIALIZATION ====================
    int lineCount = 0;
    int completedInstr = 0;
    int issuedInstr = 0;
    int writtenInstr = 0;
    int clockCycles = 0;
    int completed_rs = -1;
    double cdb_data = 0;
    bool issueSuccessful = false;
    bool cdb_busy = false;
    
    // ==================== READ IN INSTRUCTIONS ====================
    FILE *fp;
    char mystring[MAXCHAR];

    
    fp = fopen(filename, "r");
    if (fp == NULL){
        printf("Could not open file %s",filename);
        return 1;
    }
    while (fgets(mystring, MAXCHAR, fp) != NULL){
        const char t[2] = "\t";
        const char s[2] = " ";
        char *token;
        //cpoying instruction type
        token = strtok(mystring, t);
        instruction_list[lineCount].num = atof(token);
        strcpy(instruction_list[lineCount].type, strtok(token, s));
        // copying destination register
        token = strtok(NULL, s);
        strcpy(instruction_list[lineCount].dest_reg, token);
        // copying operand j
        token = strtok(NULL, s);
        if (strcmp(instruction_list[lineCount].type, "LD") == 0) {
            instruction_list[lineCount].load = atoi(token);
        }
        else {
            strcpy(instruction_list[lineCount].reg_j, token);
        }
        
        /* read in last part of instruction, if not load or store */
        if (strcmp(instruction_list[lineCount].type, "LD") == 0 or strcmp(instruction_list[lineCount].type, "SD") == 0)
        {
            strcpy(instruction_list[lineCount].reg_k, " ");
        }
        else
        {
            token = strtok(NULL, s);
            strcpy(instruction_list[lineCount].reg_k, token);
        }

        lineCount += 1;
    }
    fclose(fp);
    
    // ==================== MAIN SIMULATION LOOP ====================
    
    while (writtenInstr < lineCount)
    {
        // ================== WRITING INSTRUCTIONS ==================
        if (completed_rs != -1) {
            // broadcasting data
            for (int i=0; i < addReservationStations; i++) {
                if (add_rs[i].tag_j == completed_rs) {
                    add_rs[i].data_j = cdb_data;
                    add_rs[i].tag_j = 0;
                }
                if (add_rs[i].tag_k == completed_rs) {
                    add_rs[i].data_k = cdb_data;
                    add_rs[i].tag_k = 0;
                }
                
                // clear reservation station
                if (add_rs[i].num == completed_rs) {
                    add_rs[i].busy = false;
                    add_rs[i].cycle_count = 0;
                    add_rs[i].cycles_required = -999;
                    add_rs[i].executing = false;
                }
            }
            for (int i=0; i < mulReservationStations; i++) {
                if (mul_rs[i].tag_j == completed_rs) {
                    mul_rs[i].data_j = cdb_data;
                    mul_rs[i].tag_j = 0;
                }
                if (mul_rs[i].tag_k == completed_rs) {
                    mul_rs[i].data_k = cdb_data;
                    mul_rs[i].tag_k = 0;
                }
                    
                // clear reservation station
                if (mul_rs[i].num == completed_rs) {
                    mul_rs[i].busy = false;
                    mul_rs[i].cycle_count = 0;
                    mul_rs[i].cycles_required = -999;
                    mul_rs[i].executing = false;
                }
            }
            for (int i=0; i < loadReservationStations; i++) {
                if (load_rs[i].tag == completed_rs) {
                    load_rs[i].address = cdb_data;
                    load_rs[i].tag = 0;
                    //load_rs[i].executing = true;
                }
                // clear reservation station
                if (load_rs[i].num == completed_rs) {
                    load_rs[i].busy = false;
                    load_rs[i].cycle_count = 0;
                    load_rs[i].cycles_required = -999;
                    load_rs[i].executing = false;
                }
            }
            for (int i=0; i < storeReservationStations; i++) {
                if (store_rs[i].tag == completed_rs) {
                    store_rs[i].address = cdb_data;
                    store_rs[i].tag = 0;
                    //store_rs[i].executing = true;
                }
                // clear reservation station
                if (store_rs[i].num == completed_rs) {
                    store_rs[i].busy = false;
                    store_rs[i].cycle_count = 0;
                    store_rs[i].cycles_required = -999;
                    store_rs[i].executing = false;
                }
            }
            // write to memory
            for (int i=0; i < numRegisters; i++) {
                if (data_registers[i].tag == completed_rs) {
                    data_registers[i].data = cdb_data;
                    data_registers[i].busy = false;
                    data_registers[i].tag = 0;
                }
            }

            // just for bookkeeping - instruction written cycle number
            for (int j=0; j < issuedInstr; j++) {
                if (instruction_list[j].rs == completed_rs) {
                    if (instruction_list[j].written == -1) {
                        instruction_list[j].written = clockCycles+1;
                    }
                }
            }

            writtenInstr += 1;
            // reset
            completed_rs = -1;
            cdb_data = 0;
        }
    
        
        // ================== ISSUING INSTRUCTIONS ==================
        if (issuedInstr < lineCount) {
            // issue instruction 0...then 1...then n..etc. (& increment instruction cycle if successful)
            if (strcmp(instruction_list[issuedInstr].type, "ADDD") == 0 || strcmp(instruction_list[issuedInstr].type, "SUBD") == 0) {
                // if addition or subtraction, adding to reservation station
                for (int l=0; l < addReservationStations; l++) {
                    if (issueSuccessful == false) {
                        if (add_rs[l].busy == false) {
                            for (int i=0; i < numRegisters; i++) {
                                // j operand
                                if (strcmp(instruction_list[issuedInstr].reg_j, data_registers[i].name) == 0) {
                                        add_rs[l].data_j = data_registers[i].data;
                                    if (data_registers[i].tag == 0) {
                                    }
                                    else {
                                        add_rs[l].tag_j = data_registers[i].tag;
                                    }
                                }
                                // k operand
                                if (strncmp(instruction_list[issuedInstr].reg_k, data_registers[i].name, 2) == 0) {
                                    if (data_registers[i].tag == 0) {
                                        add_rs[l].data_k = data_registers[i].data;
                                    }
                                    else {
                                        add_rs[l].tag_k = data_registers[i].tag;
                                    }
                                }
                                // destination register
                                if (strcmp(instruction_list[issuedInstr].dest_reg, data_registers[i].name) == 0) {
                                    data_registers[i].tag = add_rs[l].num;
                                }
                            }
                            add_rs[l].busy = true;
                            instruction_list[issuedInstr].rs = add_rs[l].num;
                            add_rs[l].cycle_count = 0;
                            instruction_list[issuedInstr].issue = clockCycles+1;
                            issueSuccessful = true;
                            if (strcmp(instruction_list[issuedInstr].type, "ADDD") == 0) {
                                strcpy(add_rs[l].name, "ADDD");
                                add_rs[l].cycles_required = addCycles;
                                
                            }
                            else {
                                add_rs[l].cycles_required = subCycles;
                                strcpy(add_rs[l].name, "SUBD");
                            }
                        }
                    }
                }
            }
            // if loading
            else if (strcmp(instruction_list[issuedInstr].type, "LD") == 0) {
                for (int l=0; l < loadReservationStations; l++) {
                    if (issueSuccessful == false) {
                        if (load_rs[l].busy == false) {
                            for (int i=0; i < numRegisters; i++) {
                                // load value
                                load_rs[l].address = instruction_list[issuedInstr].load;
                                // destination register
                                if (strncmp(instruction_list[issuedInstr].dest_reg, data_registers[i].name, 2) == 0) {
                                    data_registers[i].tag = load_rs[l].num;
                                }
                            }
                            load_rs[l].busy = true;
                            load_rs[l].cycles_required = loadCycles;
                            instruction_list[issuedInstr].rs = load_rs[l].num;
                            load_rs[l].cycle_count = 0;
                            instruction_list[issuedInstr].issue = clockCycles+1;
                            issueSuccessful = true;
                        }
                    }
                }
            }
            // if storing
            else if (strcmp(instruction_list[issuedInstr].type, "SD") == 0) {
                for (int l=0; l < storeReservationStations; l++) {
                    if (issueSuccessful == false) {
                        if (store_rs[l].busy == false) {
                            for (int i=0; i < numRegisters; i++) {
                                if (strcmp(instruction_list[issuedInstr].dest_reg, data_registers[i].name) == 0) {
                                    if (data_registers[i].tag == 0) {
                                        store_rs[l].address = data_registers[i].data;
                                    }
                                    else {
                                        store_rs[l].tag = data_registers[i].tag;
                                    }
                                }
                                // destination register
                                if (strncmp(instruction_list[issuedInstr].reg_j, data_registers[i].name, 2) == 0) {
                                    data_registers[i].tag = store_rs[l].num;
                                }
                            }
                            store_rs[l].busy = true;
                            store_rs[l].cycles_required = storeCycles;
                            instruction_list[issuedInstr].rs = store_rs[l].num;
                            store_rs[l].cycle_count = 0;
                            instruction_list[issuedInstr].issue = clockCycles+1;
                            issueSuccessful = true;
                        }
                    }
                }
            }
            // if multiplication or division
            else {
                // adding to reservation station
                for (int l=0; l < mulReservationStations; l++) {
                    if (issueSuccessful == false) {
                        if (mul_rs[l].busy == false) {
                            for (int i=0; i < numRegisters; i++) {
                                // destination register
                                if (strcmp(instruction_list[issuedInstr].dest_reg, data_registers[i].name) == 0) {
                                    data_registers[i].tag = mul_rs[l].num;
                                }
                                // j operand
                                if (strcmp(instruction_list[issuedInstr].reg_j, data_registers[i].name) == 0) {
                                    if (data_registers[i].tag == 0) {
                                        mul_rs[l].data_j = data_registers[i].data;
                                    }
                                    else {
                                        mul_rs[l].tag_j = data_registers[i].tag;
                                    }
                                }
                                // k operand
                                if (strncmp(instruction_list[issuedInstr].reg_k, data_registers[i].name, 2) == 0) {
                                    if (data_registers[i].tag == 0) {
                                        mul_rs[l].data_k = data_registers[i].data;
                                    }
                                    else {
                                        mul_rs[l].tag_k = data_registers[i].tag;
                                    }
                                }
                            }
                            mul_rs[l].busy = true;
                            instruction_list[issuedInstr].rs = mul_rs[l].num;
                            mul_rs[l].cycle_count = 0;
                            instruction_list[issuedInstr].issue = clockCycles+1;
                            issueSuccessful = true;
                            if (strcmp(instruction_list[issuedInstr].type, "MULTD") == 0) {
                                strcpy(mul_rs[l].name, "MULTD");
                                mul_rs[l].cycles_required = multCycles;
                                
                            }
                            else {
                                mul_rs[l].cycles_required = diviCycles;
                                strcpy(mul_rs[l].name, "DIVD");
                            }
                        }
                    }
                }
            }
            
        }

        
        // ================== COMPLETING AND EXECUTING INSTRUCTION CHECK ==================
        // ordered by increasing reservation station number
        cdb_busy = false;
        // add reservations
        for (int i=0; i < addReservationStations; i++){

            // executing instructions
            if (add_rs[i].busy and add_rs[i].executing) {
                if (add_rs[i].cycle_count < add_rs[i].cycles_required) {
                    add_rs[i].cycle_count += 1;
                }
            }
            // setting mark of beginning of execution
                        for (int j=0; j < issuedInstr; j++) {
                    
                                if (instruction_list[j].rs == add_rs[i].num) {
                                    if (add_rs[i].cycle_count==1) {
                                        instruction_list[j].start = clockCycles+1;
                                    }
                                }
                            }
            // completing instruction
            if (add_rs[i].cycle_count == add_rs[i].cycles_required) {
                            for (int j=0; j < issuedInstr; j++) {
                                // just tracking completion cycle - simply for bookkeeping
                                if (instruction_list[j].rs == add_rs[i].num) {
                                    if (instruction_list[j].completion == -1) {
                                        instruction_list[j].completion = clockCycles+1;
                                        completed_rs = add_rs[i].num;
                                        completedInstr += 1;
                                    }
                                }
                            }
                            //writing to CDB
                            if (!cdb_busy) {
                    if (strcmp(add_rs[i].name, "ADDD") == 0) {
                        cdb_data = add_rs[i].data_j + add_rs[i].data_k;
                    }
                    else {
                        cdb_data = add_rs[i].data_j - add_rs[i].data_k;
                    }
                    cdb_busy = true;
                    //completed_rs = add_rs[i].num;
                }
                        }
        }
        // mult reservations
        for (int i=0; i < mulReservationStations; i++) {

            // executing instructions
            if (mul_rs[i].busy and mul_rs[i].executing) {
                // only increment if not yet reached
                if (mul_rs[i].cycle_count < mul_rs[i].cycles_required) {
                    mul_rs[i].cycle_count += 1;
                }
            }
            // setting mark of beginning of execution
            for (int j=0; j < issuedInstr; j++) {
                    
                    if (instruction_list[j].rs == mul_rs[i].num) {
                        if (mul_rs[i].cycle_count==1) {
                            instruction_list[j].start = clockCycles+1;
                        }
                        
                    }
                }
            // completing instruction
            if (mul_rs[i].cycle_count == mul_rs[i].cycles_required) {
                            for (int j=0; j < issuedInstr; j++) {
                                // just tracking completion cycle - simply for bookkeeping
                                if (instruction_list[j].rs == mul_rs[i].num) {
                                    if (instruction_list[j].completion == -1) {
                                        instruction_list[j].completion = clockCycles+1;
                                        completed_rs = mul_rs[i].num;
                                        completedInstr += 1;
                                    }
                                }
                            }
                            //writing to CDB
                            if (!cdb_busy) {
                                if (strcmp(mul_rs[i].name, "MULTD") == 0) {
                                    cdb_data = mul_rs[i].data_j * mul_rs[i].data_k;
                                }
                                else {
                                    cdb_data = mul_rs[i].data_j / mul_rs[i].data_k;
                                }
                                cdb_busy = true;
                                //completed_rs = mul_rs[i].num;
                            }
                        }
        }
        // load reservation
        for (int i=0; i < loadReservationStations; i++) {

            // executing instructions
            if (load_rs[i].busy and load_rs[i].executing) {
                // only increment if not yet reached
                if (load_rs[i].cycle_count < load_rs[i].cycles_required) {
                    load_rs[i].cycle_count += 1;
                }
            }
            // setting mark of beginning of execution
            for (int j=0; j < issuedInstr; j++) {
                    
                    if (instruction_list[j].rs == load_rs[i].num) {
                        if (load_rs[i].cycle_count==1) {
                            instruction_list[j].start = clockCycles+1;
                        }
                        
                    }
                }
            // completing instruction
            if (load_rs[i].cycle_count == load_rs[i].cycles_required) {
                            for (int j=0; j < issuedInstr; j++) {
                                // just tracking completion cycle - simply for bookkeeping
                                if (instruction_list[j].rs == load_rs[i].num) {
                                    if (instruction_list[j].completion == -1) {
                                        instruction_list[j].completion = clockCycles+1;
                                        completed_rs = load_rs[i].num;
                                        completedInstr += 1;
                                    }
                                }
                            }
                            //writing to CDB
                            if (!cdb_busy) {
                                                cdb_data = load_rs[i].address;
                                                cdb_busy = true;
                                                //completed_rs = load_rs[i].num;
                                            }
                        }
        }
        // store reservation
        for (int i=0; i < storeReservationStations; i++) {

            
            // executing instructions
            if (store_rs[i].busy and store_rs[i].executing) {
                cout << "cycle count" << store_rs[i].cycle_count << endl;
                cout << "cycles required" << store_rs[i].cycles_required << endl;
                // only increment if not yet reached
                if (store_rs[i].cycle_count < store_rs[i].cycles_required) {
                    store_rs[i].cycle_count += 1;
                }
            }
            // setting mark of beginning of execution
            for (int j=0; j < issuedInstr; j++) {
                    
                    if (instruction_list[j].rs == store_rs[i].num) {
                        if (store_rs[i].cycle_count==1) {
                            instruction_list[j].start = clockCycles+1;
                        }
                        
                    }
                }
            //completing instruction
            if (store_rs[i].cycle_count == store_rs[i].cycles_required) {
                for (int j=0; j < issuedInstr; j++) {
                    // just tracking completion cycle - simply for bookkeeping
                    if (instruction_list[j].rs == store_rs[i].num) {
                        if (instruction_list[j].completion == -1) {
                            instruction_list[j].completion = clockCycles+1;
                            completed_rs = store_rs[i].num;
                            completedInstr += 1;
                        }
                    }
                }
                //writing to CDB
                if (!cdb_busy) {
                                    cdb_data = store_rs[i].address;
                                    cdb_busy = true;
                                    //completed_rs = store_rs[i].num;
                                }
            }
        }
        
        for (int l=0; l < addReservationStations; l++) {
        	if (add_rs[l].busy == true) {
        		if (add_rs[l].tag_j == 0 and add_rs[l].tag_k == 0) {
                        add_rs[l].executing = true;
                           }
				} 
		} 
		for (int l=0; l < loadReservationStations; l++){
			if (load_rs[l].busy == true){
				load_rs[l].executing = true;
			}
		}
		for (int l=0; l < storeReservationStations; l++) {
			if (store_rs[l].busy == true) {
        		if (store_rs[l].tag == 0) {
                    	store_rs[l].executing = true;
                         }
				} 
		}
		for (int l=0; l < mulReservationStations; l++) {
			if (mul_rs[l].busy == true) {
				if (mul_rs[l].tag_j == 0 and mul_rs[l].tag_k == 0) {
                        mul_rs[l].executing = true;
                           }
			}	
		}
		
    
        // ================== PRINTING TO CONSOLE ==================
        printInstructionStatus("test", 6);
        for (int j=0; j < lineCount; j++) {
            printElement(instruction_list[j].type, 15);
            if (strcmp(instruction_list[j].type, "LD") == 0) {
                printElement(instruction_list[j].load, 6);
                printElement(" ", 8);
            }
            else {
                printElement(instruction_list[j].reg_j[0], 0);
                printElement(instruction_list[j].reg_j[1], 6);
                printElement(instruction_list[j].reg_k[0], 0);
                printElement(instruction_list[j].reg_k[1], 6);
            }
            if (instruction_list[j].issue == -1) {
                printElement(" ", 8);
            }
            else printElement(instruction_list[j].issue, 8);
            if (instruction_list[j].start!=0)
            {
            	printElement(instruction_list[j].start,8);
			}
			else printElement(" ", 8);
            if (instruction_list[j].completion == -1) {
                printElement(" ", 12);
            }
            else printElement(instruction_list[j].completion, 12);
            if (instruction_list[j].written == -1) {
                printElement(" ", 0);
            }
            else printElement(instruction_list[j].written, 0);
            cout << endl;
        }
        printStationStatus("test", 6);
        for (int i=0; i < addReservationStations; i++) {
            if (add_rs[i].cycles_required == -999) {
                printElement("", 8);
            }
            else {
                printElement(add_rs[i].cycles_required - add_rs[i].cycle_count, 8);
            }
            printElement("[Add", 0);
            printElement(add_rs[i].num, 0);
            printElement("]", 2);
            if (add_rs[i].busy == 0) {
                printElement(" ", 8);
                printElement(" ", 8);
                printElement(" ", 8);
                printElement(" ", 8);
            }
            else {
                printElement(add_rs[i].busy, 8);
                printElement(add_rs[i].name, 8);
                printElement(add_rs[i].tag_j, 8);
                printElement(add_rs[i].data_j, 8);
                printElement(add_rs[i].tag_k, 8);
                printElement(add_rs[i].data_k, 8);
            }
            cout << endl;
        }
        for (int i=0; i < mulReservationStations; i++) {
            if (mul_rs[i].cycles_required == -999) {
                printElement("", 8);
            }
            else {
                printElement(mul_rs[i].cycles_required - mul_rs[i].cycle_count, 8);
            }
            printElement("[Mult", 0);
            printElement(mul_rs[i].num-3, 0);
            printElement("]", 2);
            if (mul_rs[i].busy == 0) {
                printElement(" ", 8);
                printElement(" ", 8);
                printElement(" ", 8);
                printElement(" ", 8);
            }
            else {
                printElement(mul_rs[i].busy, 8);
                printElement(mul_rs[i].name, 8);
                printElement(mul_rs[i].tag_j, 8);
                printElement(mul_rs[i].data_j, 8);
                printElement(mul_rs[i].tag_k, 8);
                printElement(mul_rs[i].data_k, 8);
            }
            cout << endl;
        }
        printLoadStatus("test", 6);
        for (int i=0; i < loadReservationStations; i++) {
            printElement("", 8);
            printElement("[Load", 0);
            printElement(load_rs[i].num-5, 0);
            printElement("]", 2);
            printElement(load_rs[i].busy, 10);
            if (load_rs[i].busy == true) {
                printElement(load_rs[i].address, 0);
            }
            cout << endl;
        }
        printStoreStatus("test", 6);
        for (int i=0; i < storeReservationStations; i++) {
            printElement("", 8);
            printElement("[Stor", 0);
            printElement(store_rs[i].num-8, 0);
            printElement("]", 2);
            printElement(store_rs[i].busy, 10);
            if (store_rs[i].busy == true) {
                printElement(store_rs[i].address, 0);
            }
            cout << endl;
        }
        printRegisterStatus("test", 6);
        printElement(clockCycles+1, 8);
        for (int i=0; i < numRegisters; i++) {
            if (data_registers[i].tag == 0) {
                printElement(data_registers[i].data, 8);
            }
            else {
                printElement("[", 0);
                printElement(data_registers[i].tag, 0);
                printElement("]", 8);
            }
        }
        cout << endl << endl;
        
        // ================== INCREMENT COUNTERS ==================
        if (issueSuccessful) {
            issuedInstr += 1;
            issueSuccessful = false;
        }
        clockCycles += 1;
    }
    
    return 0;
}


// ================== PRINT FUNCTIONS ==================
void header(int n)
{
    cout << "Cycle " << n << endl << endl;
}

template<typename T> void printElement(T t, const int& width)
{
    const char separator    = ' ';
    cout << left << setw(width) << setfill(separator) << t;
}

template<typename T> void printInstructionStatus(T t, const int& width)
{
    cout << "Instruction Status:" << endl;
    printElement("Instruction", 15);
    printElement("j", 6);
    printElement("k", 6);
    printElement("Issue", 8);
    printElement("Start", 8);
    printElement("Completion", 12);
    printElement("Written", 0);
    cout << endl;
}

template<typename T> void printLoadStatus(T t, const int& width)
{
    cout << endl << "Load Status:" << endl;
    printElement("", 8);
    printElement("Name", 8);
    printElement("Busy", 10);
    printElement("Address", 0);
    cout << endl;
}

template<typename T> void printStoreStatus(T t, const int& width)
{
    cout << endl << "Store Status:" << endl;
    printElement("", 8);
    printElement("Name", 8);
    printElement("Busy", 10);
    printElement("Address", 0);
    cout << endl;
}

template<typename T> void printStationStatus(T t, const int& width)
{
    cout << endl << "Reservation Stations:" << endl;
    printElement("Time", 8);
    printElement("Name", 8);
    printElement("Busy", 8);
    printElement("Op", 8);
    printElement("Qj", 8);
    printElement("Vj", 8);
    printElement("Qk", 8);
    printElement("Vk", 8);
    cout << endl;
}

template<typename T> void printRegisterStatus(T t, const int& width)
{
    cout << endl << "Register Result Status:" << endl;
    printElement("Clock", 8);
    printElement("R0", 8);
    printElement("R2", 8);
    printElement("R4", 8);
    printElement("R6", 8);
    printElement("R8", 8);
    printElement("R10", 8);
    cout << endl;
}
    

