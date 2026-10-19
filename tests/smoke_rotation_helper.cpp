#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
int main(int argc,char** argv) {
  std::filesystem::path log;
  for(int i=1;i+1<argc;++i) if(std::string(argv[i])=="--log_file") log=argv[i+1];
  if(log.empty()) return 2;
  { std::ofstream f(log);f<<"Direct3D 12 device and OS features\nPvZ frame replay delivered row 0 step 0\n"; }
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  auto rotated=log.parent_path()/(log.stem().string()+".1.log");
  std::filesystem::rename(log,rotated);
  {std::ofstream f(rotated,std::ios::app);f<<"Captured guest frame 1280x720 to rotation.ppm\nPvZ frame replay completed rotation.csv\n";}
  {std::ofstream f(log);f<<"active log after rotation\n";}
  std::this_thread::sleep_for(std::chrono::seconds(30));
}
