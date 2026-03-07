// Verilator Example
// Norbertas Kremeris 2021
#include <stdlib.h>
#include <iostream>
#include <cstdlib>
#include <memory>
#include <verilated.h>
#include <verilated_fst_c.h>
#include "Valu.h"
#include "Valu___024unit.h"

#define MAX_SIM_TIME 300
#define VERIF_START_TIME 7
vluint64_t sim_time = 0;
vluint64_t posedge_cnt = 0;

// ALU input interface transaction item class
class AluInTx {
    public:
        uint32_t a;
        uint32_t b;
        enum Operation {
            add = Valu___024unit::operation_t::add,
            sub = Valu___024unit::operation_t::sub,
            nop = Valu___024unit::operation_t::nop
        } op;
};

// ALU output interface transaction item class
class AluOutTx {
    public:
        uint32_t out;
};

// ALU scoreboard
class AluScb {
    private:
        std::deque<std::unique_ptr<AluInTx>> in_q;
        
    public:
        // Input interface monitor port
        void writeIn(std::unique_ptr<AluInTx> tx){
            // Push the received transaction item into a queue for later
            in_q.emplace_back(std::move(tx));
        }

        // Output interface monitor port
        void writeOut(std::unique_ptr<AluOutTx> tx){
            // We should never get any data from the output interface
            // before an input gets driven to the input interface
            if(in_q.empty()){
                std::cout <<"Fatal Error in AluScb: empty AluInTx queue" << std::endl;
                exit(1);
            }

            // Grab the transaction item from the front of the input item queue
            std::unique_ptr<AluInTx> in{std::move(in_q.front())};
            in_q.pop_front();

            switch(in->op){
                // A valid signal should not be created at the output when there is no operation,
                // so we should never get a transaction item where the operation is NOP
                case AluInTx::nop :
                    std::cout << "Fatal error in AluScb, received NOP on input" << std::endl;
                    exit(1);
                    break;

                // Received transaction is add
                case AluInTx::add :
                    if (in->a + in->b != tx->out) {
                        std::cout << std::endl;
                        std::cout << "AluScb: add mismatch" << std::endl;
                        std::cout << "  Expected: " << in->a + in->b
                                  << "  Actual: " << tx->out << std::endl;
                        std::cout << "  Simtime: " << sim_time << std::endl;
                    }
                    break;

                // Received transaction is sub
                case AluInTx::sub :
                    if (in->a - in->b != tx->out) {
                        std::cout << std::endl;
                        std::cout << "AluScb: sub mismatch" << std::endl;
                        std::cout << "  Expected: " << in->a - in->b
                                  << "  Actual: " << tx->out << std::endl;
                        std::cout << "  Simtime: " << sim_time << std::endl;
                    }
                    break;
            }
        }
};

// ALU input interface driver
class AluInDrv {
    private:
        std::shared_ptr<Valu> dut;
    public:
        AluInDrv(std::shared_ptr<Valu> dut){
            this->dut = dut;
        }

        void drive(std::unique_ptr<AluInTx> tx){
            // we always start with in_valid set to 0, and set it to
            // 1 later only if necessary
            dut->in_valid = 0;

            // Don't drive anything if a transaction item doesn't exist
            if(tx != NULL){
                if (tx->op != AluInTx::nop) {
                    // If the operation is not a NOP, we drive it onto the
                    // input interface pins
                    dut->in_valid = 1;
                    dut->op_in = tx->op;
                    dut->a_in = tx->a;
                    dut->b_in = tx->b;
                }
            }
        }
};

// ALU input interface monitor
class AluInMon {
    private:
        std::shared_ptr<Valu> dut;
        std::shared_ptr<AluScb> scb;
    public:
        AluInMon(std::shared_ptr<Valu> dut, std::shared_ptr<AluScb> scb){
            this->dut = dut;
            this->scb = scb;
        }

        void monitor(){
            if (dut->in_valid == 1) {
                // If there is valid data at the input interface,
                // create a new AluInTx transaction item and populate
                // it with data observed at the interface pins
                auto tx{std::make_unique<AluInTx>()};
                tx->op = AluInTx::Operation(dut->op_in);
                tx->a = dut->a_in;
                tx->b = dut->b_in;

                // then pass the transaction item to the scoreboard
                scb->writeIn(std::move(tx));
            }
        }
};

// ALU output interface monitor
class AluOutMon {
    private:
        std::shared_ptr<Valu> dut;
        std::shared_ptr<AluScb> scb;
    public:
        AluOutMon(std::shared_ptr<Valu> dut, std::shared_ptr<AluScb> scb){
            this->dut = dut;
            this->scb = scb;
        }

        void monitor(){
            if (dut->out_valid == 1) {
                // If there is valid data at the output interface,
                // create a new AluOutTx transaction item and populate
                // it with result observed at the interface pins
                auto tx{std::make_unique<AluOutTx>()};
                tx->out = dut->out;

                // then pass the transaction item to the scoreboard
                scb->writeOut(std::move(tx));
            }
        }
};

// ALU random transaction generator
// This will allocate memory for an AluInTx
// transaction item, randomise the data, and
// return a pointer to the transaction item object
std::unique_ptr<AluInTx> rndAluInTx(){
    //20% chance of generating a transaction
    if(rand()%5 == 0){
        auto tx{std::make_unique<AluInTx>()};
        tx->op = AluInTx::Operation(rand() % 3); // Our ENUM only has entries with values 0, 1, 2
        tx->a = rand() % 11 + 10; // generate a in range 10-20
        tx->b = rand() % 6;  // generate b in range 0-5
        return tx;
    } else {
        return NULL;
    }
}


void dut_reset (std::shared_ptr<Valu> dut, vluint64_t &sim_time){
    dut->rst = 0;
    if(sim_time >= 3 && sim_time < 6){
        dut->rst = 1;
        dut->a_in = 0;
        dut->b_in = 0;
        dut->op_in = 0;
        dut->in_valid = 0;
    }
}

int main(int argc, char** argv, char** env) {
    srand (time(NULL));
    Verilated::commandArgs(argc, argv);
    const auto dut{std::make_shared<Valu>()};

    Verilated::traceEverOn(true);
    const auto m_trace{std::make_unique<VerilatedFstC>()};
    dut->trace(m_trace.get(), 5);
    m_trace->open("waveform.fst");

    std::unique_ptr<AluInTx> tx;

    // Here we create the driver, scoreboard, input and output monitor blocks
    const auto drv{std::make_unique<AluInDrv>(dut)};
    const auto scb{std::make_shared<AluScb>()};
    const auto inMon{std::make_unique<AluInMon>(dut, scb)};
    const auto outMon{std::make_unique<AluOutMon>(dut, scb)};

    while (sim_time < MAX_SIM_TIME) {
        dut_reset(dut, sim_time);
        dut->clk ^= 1;
        dut->eval();

        // Do all the driving/monitoring on a positive edge
        if (dut->clk == 1){

            if (sim_time >= VERIF_START_TIME) {
                // Generate a randomised transaction item of type AluInTx
                tx = rndAluInTx();

                // Pass the transaction item to the ALU input interface driver,
                // which drives the input interface based on the info in the
                // transaction item
                drv->drive(std::move(tx));

                // Monitor the input interface
                inMon->monitor();

                // Monitor the output interface
                outMon->monitor();
            }
        }
        // end of positive edge processing

        m_trace->dump(sim_time);
        sim_time++;
    }

    m_trace->close();
    exit(EXIT_SUCCESS);
}
