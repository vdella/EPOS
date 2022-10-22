// EPOS-- RISC-V 64 MMU Mediator Implementation

#include <architecture/rv64/rv64_mmu.h>

__BEGIN_SYS

// Class attributes
MMU::List MMU::_free;
MMU::Page_Directory * MMU::_master;

__END_SYS
