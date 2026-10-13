#define PM1_IDLE_NATIVE
#include "pm1_idle.h"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>

struct PM1Bus {
  std::array<uint8_t,256> regs{};
  bool asleep=false,dead=false,dropEnable=false,rejectEnable=false;
  unsigned reads=0,writes=0,pauses=0,failReads=0;
  std::vector<uint8_t> writtenRegisters;
  bool read(uint8_t reg,uint8_t *data,size_t size) {
    ++reads;
    if(dead)return false;
    if(asleep){asleep=false;return false;}
    if(failReads){--failReads;return false;}
    for(size_t i=0;i<size;++i)data[i]=regs[reg+i];
    return true;
  }
  bool write(uint8_t reg,uint8_t value) {
    ++writes;writtenRegisters.push_back(reg);
    if(dead)return false;
    if(reg==0x09 && (value&15) && rejectEnable)return false;
    if(!(reg==0x09 && (value&15) && dropEnable))regs[reg]=value;
    return true;
  }
  void pause(){++pauses;}
};

int main() {
  {
    PM1Bus bus;bus.regs[0x09]=0xB0;bus.regs[0x06]=0x16;bus.regs[0x07]=0x20;
    PM1Idle<PM1Bus> idle(bus);
    assert(idle.configure());assert(bus.regs[0x09]==0xB2);
    assert(idle.stats().configured && idle.stats().seconds==2);
    // No output/hold registers change when idle sleep is enabled.
    assert(bus.regs[0x06]==0x16 && bus.regs[0x07]==0x20);
    assert(bus.writtenRegisters==std::vector<uint8_t>{0x09});
    bus.asleep=true;unsigned callbacks=0;
    assert(idle.run([&](){++callbacks;return true;}));
    assert(callbacks==1 && bus.pauses==1);
    assert(idle.stats().wakeRetries==1 && idle.stats().recoveredReads==1);
    uint8_t data[2]={};bus.asleep=true;bus.regs[0x22]=0x55;bus.regs[0x23]=0x06;
    assert(idle.read(0x22,data,2));assert(data[0]==0x55 && data[1]==0x06);
    assert(idle.stats().recoveredReads==2);
  }
  {
    PM1Bus bus;PM1Idle<PM1Bus> idle(bus);bus.dead=true;
    unsigned calls=0;
    assert(!idle.run([&](){++calls;return true;}));
    assert(calls==0 && bus.reads==2 && bus.pauses==1 && bus.writes==0);
    assert(!idle.write(0x0C,0xA1));assert(bus.writes==0);
    assert(!idle.configure());
    assert(!idle.stats().configured && !idle.stats().fallbackVerified);
    assert(bus.writes==0 && bus.reads==8 && bus.pauses==4);
  }
  for(bool reject:{false,true}) {
    PM1Bus bus;bus.regs[0x09]=0xD0;
    bus.dropEnable=!reject;bus.rejectEnable=reject;
    PM1Idle<PM1Bus> idle(bus);
    assert(!idle.configure());
    assert(bus.regs[0x09]==0xD0 && bus.writes==2);
    assert(!idle.stats().configured && idle.stats().fallbackVerified);
    assert(idle.stats().seconds==0 && idle.stats().fallbacks==1);
    assert(idle.stats().writeFailures==unsigned(reject));
  }
  {
    PM1Bus bus;bus.regs[0x09]=0x12;
    PM1Idle<PM1Bus> idle(bus);
    // Invalid configuration fails closed and preserves the bus-speed bit.
    assert(!idle.configure(16));assert(bus.regs[0x09]==0x10);
    assert(idle.stats().fallbackVerified);
    assert(idle.configure(0));assert(idle.stats().configured && idle.stats().seconds==0);
  }
  {
    PM1Bus bus;PM1Idle<PM1Bus> idle(bus);bus.regs[0x07]=0xAB;bus.asleep=true;
    assert(idle.masked(0x07,0x60,0x20));assert(bus.regs[0x07]==0xAB);
    bus.regs[0x07]=0xCB;assert(idle.masked(0x07,0x60,0x20));assert(bus.regs[0x07]==0xAB);
    bus.dead=true;assert(!idle.masked(0x07,0x60,0));assert(bus.regs[0x07]==0xAB);
  }
  {
    PM1Bus bus;PM1Idle<PM1Bus> idle(bus);unsigned shutdowns=0;
    // A high-level/destructive operation is never repeated on failure.
    assert(!idle.run([&](){++shutdowns;return false;}));assert(shutdowns==1);
    assert(idle.stats().operationFailures==1 && idle.stats().wakeRetries==0);
  }
  std::cout<<"PM1 idle wake, bounded faults, verified fallback and rail preservation passed\n";
}
