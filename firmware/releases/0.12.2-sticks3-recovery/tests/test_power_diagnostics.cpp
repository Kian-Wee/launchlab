#include "../LaunchLabMini/power_diagnostics.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace PowerDiagnostics;Image v;fresh(v);assert(valid(v));
 account(v,UINT32_MAX,Charging|Led|Boost,100);account(v,20,0,0);
 assert(v.awakeMs==uint64_t(UINT32_MAX)+20 && v.chargingMs==UINT32_MAX && v.displayMs==UINT32_MAX);
 for(unsigned i=0;i<CAPACITY+9;++i)append(v,Sample,3700+i,ChargeKnown|SensorRail,100,240);
 assert(valid(v) && v.count==CAPACITY && v.next==9 && v.records[8].batteryMv==3756);
 Image corrupt=v;corrupt.records[0].batteryMv^=1;assert(!valid(corrupt));
 corrupt=v;corrupt.count=CAPACITY+1;corrupt.checksum=hash(corrupt);assert(!valid(corrupt));
 corrupt=v;corrupt.next=CAPACITY;corrupt.checksum=hash(corrupt);assert(!valid(corrupt));
 std::cout<<"PASS: power log ring rollover, 64-bit durations, state attribution and corruption rejection\n";
}
