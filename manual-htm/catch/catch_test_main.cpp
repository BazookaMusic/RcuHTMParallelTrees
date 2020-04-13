#define CATCH_CONFIG_RUNNER
#include "../../include/catch2/catch.hpp"
#include "../../include/test_bench_conf.hpp"
#include <string>

int TREE_SIZE;
int THREAD_COUNT;
int L;
int I;
int SEED;


int main( int argc, char* argv[] )
{
    Catch::Session session; // There must be exactly one instance

    if (argc > 2) {


        TREE_SIZE = std::stoi(std::string(argv[2]));
        THREAD_COUNT = std::stoi(std::string(argv[3]));
        L = std::stoi(std::string(argv[4]));
        I = std::stoi(std::string(argv[5]));
        SEED = std::stoi(std::string(argv[6]));

        


        for (int i = 2; i < 7; i++) argv[i] = nullptr;
        argc = 2;

        std::cout << "Tree size:" << TREE_SIZE << std::endl;
        std::cout << "THREADS:" << THREAD_COUNT << std::endl;
        std::cout << "L:" << L << std::endl;
        std::cout << "I:" << I << std::endl;
        std::cout << "R:" << 100-L-I << std::endl;
        std::cout << "SEED:" << SEED << std::endl;

    }

    



    int numFailed = session.run();
    // Note that on unices only the lower 8 bits are usually used, clamping
    // the return value to 255 prevents false negative when some multiple
    // of 256 tests has failed
    return ( numFailed < 0xff ? numFailed : 0xff );
}