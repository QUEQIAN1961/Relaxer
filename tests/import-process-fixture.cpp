// A deliberately stalled, non-network ffprobe stand-in used only by B6 tests.
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <thread>
int main(){
    const auto *marker=std::getenv("RELAXER_IMPORT_PROCESS_MARKER");
    if(!marker)return 2;
    {std::ofstream file(marker,std::ios::binary);file<<"synthetic process ready";}
    std::this_thread::sleep_for(std::chrono::seconds(30));
    return 0;
}
