// Runs the actual native implementations against verbatim generated Xbox code.
#define PVZ_NATIVE_REPLACEMENT_TEST
#define PVZ_B001_SYNTHETIC_TEST
#include "pvz_pch.h"
#include "native/replacement.h"
#include "guest_memory.h"
#include <iostream>
#include <fstream>
#include <functional>
#include <random>
namespace pvz::native { using RegisterOutput=Output<PPCContext,PPCRegister>; }
namespace {
using pvz::native::SyntheticInputs;
struct Test { const char* name; const char* description; std::function<pvz::native::SyntheticResult()> run; };
auto& Tests() { static std::vector<Test> tests;return tests; }
struct Add { Add(Test t) {Tests().push_back(std::move(t));} };
}
#define SNAP(...)
#define LEAF_None(...)
#define LEAF_CASE(A,P,B,O,R,I) \
  REX_EXTERN(__imp__sub_##A); \
  const Add test_##A{{#A,I##Inputs.description,[] {return pvz::native::VerifySynthetic(I##Inputs,0x8000,O,R,B,__imp__sub_##A);}}};
#define LEAF_Identity LEAF_CASE
#define LEAF_FocusByte LEAF_CASE
#define LEAF_Digits LEAF_CASE
#define LEAF_Color LEAF_CASE
#define LEAF(A,P,B,O,R,I) LEAF_##I(A,P,B,O,R,I)
#include "../src/native/b001_widget.cpp"
int main(int argc,char** argv) {
  std::ofstream file;
  if(argc>1) file.open(argv[1]);
  std::ostream& out=argc>1?file:std::cout;
  bool pass=Tests().size()==24;
  out<<"{\"seed\":42,\"functions\":[";
  bool first=true;
  for(const auto& test:Tests()) {
    auto r=test.run();pass &= r.cases>0 && !r.errors && r.negative_errors==r.cases;
    out<<(first?"":",")<<"{\"address\":\"0x"<<test.name<<"\",\"cases\":"<<r.cases
       <<",\"errors\":"<<r.errors<<",\"negative_errors\":"<<r.negative_errors<<"}";
    first=false;
  }
  out<<"],\"passed\":"<<(pass?"true":"false")<<"}\n";
  return pass?0:1;
}
