#ifndef __riscv_sifive_u_otp_h
#define __riscv_sifive_u_otp_h

#include <architecture/cpu.h>
#include <machine/otp.h>
#include <machine/io.h>
#include <system/memory_map.h>
#include <time.h>

__BEGIN_SYS

#define PA_RESET_VAL		    0x00
#define PAS_RESET_VAL		    0x00
#define PAIO_RESET_VAL		    0x00
#define PDIN_RESET_VAL		    0x00
#define PTM_RESET_VAL		    0x00

#define BIT(nr)                 (1UL << (nr))

#define PCLK_ENABLE_VAL			BIT(0)
#define PCLK_DISABLE_VAL		0x00

#define PWE_WRITE_ENABLE		BIT(0)
#define PWE_WRITE_DISABLE		0x00

#define PTM_FUSE_PROGRAM_VAL    BIT(1)

#define PCE_ENABLE_INPUT		BIT(0)
#define PCE_DISABLE_INPUT		0x00

#define PPROG_ENABLE_INPUT		BIT(0)
#define PPROG_DISABLE_INPUT		0x00

#define PTRIM_ENABLE_INPUT		BIT(0)
#define PTRIM_DISABLE_INPUT		0x00

#define PDSTB_DEEP_STANDBY_ENABLE	BIT(0)
#define PDSTB_DEEP_STANDBY_DISABLE	0x00

class SiFive_OTP
{
private:
    typedef CPU::Reg32 Reg32;

public:
    static const unsigned int BYTES_PER_FUSE = 4;
    static const unsigned int total_fuses = 4;

    static const unsigned int TPW_DELAY      = Traits<OTP>::TPW_DELAY;
    static const unsigned int TPWI_DELAY     = Traits<OTP>::TPWI_DELAY;
    static const unsigned int TASP_DELAY     = Traits<OTP>::TASP_DELAY;
    static const unsigned int TCD_DELAY      = Traits<OTP>::TCD_DELAY;
    static const unsigned int TKL_DELAY      = Traits<OTP>::TKL_DELAY;
    static const unsigned int TMS_DELAY      = Traits<OTP>::TMS_DELAY;

    // OTP registers offsets from OTP_BASE
    enum {
        PA      = 0x00,   // Address input
        PAIO    = 0x04,   // Program address input
        PAS     = 0x08,   // Program redundancy cell selection input
        PCE     = 0x0C,   // OTP Macro enable input
        PCLK    = 0x10,   // Clock input
        PDIN    = 0x14,   // Write data input
        PDOUT   = 0x18,   // Read data output
        PDSTB   = 0x1C,   // Deep standby mode enable input (active low)
        PPROG   = 0x20,   // Program mode enable input
        PTC     = 0x24,   // Test column enable input
        PTM     = 0x28,   // Test mode enable input
        PTM_REP = 0x2C,   // Repair function test mode enable input
        PTR     = 0x30,   // Test row enable input
        PTRIM   = 0x34,   // Repair function enable input
        PWE     = 0x38    // Write enable input (defines program cycle)
    };

public:
    SiFive_OTP() {}

    int read_shot(int offset, void *buf, int size)
    {
        // Check if offset and size are multiple of BYTES_PER_FUSE.
        if ((size % BYTES_PER_FUSE) || (offset % BYTES_PER_FUSE)) {
            return -1;
        }

        unsigned int fuse_id = offset / BYTES_PER_FUSE;
        unsigned int fuse_count = size / BYTES_PER_FUSE;

        db<OTP>(WRN) << "Started reading OTP!" << endl;

        // Check bounds.
        if (offset < 0 || size < 0)
            return -1;
        if (fuse_id >= total_fuses)
            return -1;
        if ((fuse_id + fuse_count) > total_fuses)
            return -1;

        int fuse_buf[fuse_count];

        db<OTP>(WRN) << "Gathering registers!" << endl;

        // Init OTP.
        reg(SiFive_OTP::PDSTB) = PDSTB_DEEP_STANDBY_ENABLE;
        reg(SiFive_OTP::PTRIM) = PTRIM_ENABLE_INPUT;
        reg(SiFive_OTP::PCE) = PCE_ENABLE_INPUT;

        db<OTP>(WRN) << "PDSTB = " << SiFive_OTP::PDSTB << endl;
        db<OTP>(WRN) << "PDSTB REG = " << reg(SiFive_OTP::PDSTB) << endl;
        
        db<OTP>(WRN) << "PTRIM = " << SiFive_OTP::PTRIM << endl;
        db<OTP>(WRN) << "PTRIM REG = " << reg(SiFive_OTP::PTRIM) << endl;

        db<OTP>(WRN) << "PCE = " << SiFive_OTP::PCE << endl;
        db<OTP>(WRN) << "PCE REG = " << reg(SiFive_OTP::PCE) << endl;

        // Read all requested fuses.
        for (unsigned int i = 0; i < fuse_count; i++, fuse_id++) {
            reg(SiFive_OTP::PA) = fuse_id;
            db<OTP>(WRN) << "PA = " << SiFive_OTP::PA << endl;
            db<OTP>(WRN) << "PA REG = " << reg(SiFive_OTP::PA) << endl;

            /* cycle clock to read */
            reg(SiFive_OTP::PCLK) = PCLK_ENABLE_VAL;
            db<OTP>(WRN) << "PCLK = " << SiFive_OTP::PCLK << endl;
            db<OTP>(WRN) << "PCLK REG = " << reg(SiFive_OTP::PCLK) << endl;

            Delay tcd(TCD_DELAY);

            reg(SiFive_OTP::PCLK) = PCLK_DISABLE_VAL;
            db<OTP>(WRN) << "PCLK = " << SiFive_OTP::PCLK << endl;
            db<OTP>(WRN) << "PCLK REG = " << reg(SiFive_OTP::PCLK) << endl;
            
            Delay tkl(TKL_DELAY);

            /* read the value */
            db<OTP>(WRN) << "PDOUT = " << SiFive_OTP::PDOUT << endl;
            db<OTP>(WRN) << "PDOUT REG = " << reg(SiFive_OTP::PDOUT) << endl;
            fuse_buf[i] = reg(SiFive_OTP::PDOUT);
        }

        db<OTP>(WRN) << "PCE = " << SiFive_OTP::PCE << endl;
        db<OTP>(WRN) << "PCE REG = " << reg(SiFive_OTP::PCE) << endl;

        db<OTP>(WRN) << "PTRIM = " << SiFive_OTP::PTRIM << endl;
        db<OTP>(WRN) << "PTRIM REG = " << reg(SiFive_OTP::PTRIM) << endl;

        db<OTP>(WRN) << "PDSTB = " << SiFive_OTP::PDSTB << endl;
        db<OTP>(WRN) << "PDSTB REG = " << reg(SiFive_OTP::PDSTB) << endl;

        // Shut down.
        reg(SiFive_OTP::PCE) = PCE_DISABLE_INPUT;
        reg(SiFive_OTP::PTRIM) = PTRIM_DISABLE_INPUT;
        reg(SiFive_OTP::PDSTB) = PDSTB_DEEP_STANDBY_DISABLE;

        // Copy out.
        memcpy(buf, fuse_buf, size);

        return size;
    }

    /*
    * Caution:
    * OTP can be written only once, so use carefully.
    *
    * offset and size are assumed aligned to the size of the fuses (32-bit).
    */
    int write_shot(int offset, const void *buf, int size)
    {
        // Check if offset and size are multiple of BYTES_PER_FUSE.
        if ((size % BYTES_PER_FUSE) || (offset % BYTES_PER_FUSE)) {
            return -1;
        }

        unsigned int fuse_id = offset / BYTES_PER_FUSE;
        unsigned int fuse_count = size / BYTES_PER_FUSE;
        Reg32 *write_buf = (Reg32 *) buf;
        int pas, bit;

        // Check bounds.
        if (offset < 0 || size < 0)
            return -1;
        if (fuse_id >= total_fuses)
            return -1;
        if ((fuse_id + fuse_count) > total_fuses)
            return -1;

        // Init OTP.
        reg(SiFive_OTP::PDSTB) = PDSTB_DEEP_STANDBY_ENABLE;
        reg(SiFive_OTP::PTRIM) = PTRIM_ENABLE_INPUT;
        reg(SiFive_OTP::PCLK) = PCLK_DISABLE_VAL;
        reg(SiFive_OTP::PA) = PA_RESET_VAL;
        reg(SiFive_OTP::PAS) = PAS_RESET_VAL;
        reg(SiFive_OTP::PAIO) = PAIO_RESET_VAL;
        reg(SiFive_OTP::PDIN) = PDIN_RESET_VAL;
        reg(SiFive_OTP::PWE) = PWE_WRITE_DISABLE;
        reg(SiFive_OTP::PTM) = PTM_FUSE_PROGRAM_VAL;

        Delay tms(TMS_DELAY);

        reg(SiFive_OTP::PCE) = PCE_ENABLE_INPUT;
        reg(SiFive_OTP::PPROG) = PPROG_ENABLE_INPUT;

        // Write all requested fuses.
        for (unsigned int i = 0; i < fuse_count; i++, fuse_id++) {
            reg(SiFive_OTP::PA) = fuse_id;
            Reg32 write_data = *(write_buf++);

            for (pas = 0; pas < 2; pas++) {
                reg(SiFive_OTP::PAS) = pas;

                for (bit = 0; bit < 32; bit++) {
                    reg(SiFive_OTP::PAIO) = bit;
                    reg(SiFive_OTP::PDIN) = (write_data >> bit) & 1;

                    Delay tasp(TASP_DELAY);

                    reg(SiFive_OTP::PWE) = PWE_WRITE_ENABLE;

                    // EPOS Timer
                    Delay tpw(TPW_DELAY);
                    reg(SiFive_OTP::PWE) = PWE_WRITE_DISABLE;

                    // EPOS Timer
                    Delay tpwi(TPWI_DELAY);
                }
            }

            reg(SiFive_OTP::PAS) = PAS_RESET_VAL;
        }

        // Shut down.
        reg(SiFive_OTP::PWE) = PWE_WRITE_DISABLE;
        reg(SiFive_OTP::PPROG) = PPROG_DISABLE_INPUT;
        reg(SiFive_OTP::PCE) = PCE_DISABLE_INPUT;
        reg(SiFive_OTP::PTM) = PTM_RESET_VAL;
        reg(SiFive_OTP::PTRIM) = PTRIM_DISABLE_INPUT;
        reg(SiFive_OTP::PDSTB) = PDSTB_DEEP_STANDBY_DISABLE;

        return size;
    }

private:
    static volatile CPU::Reg32 & reg(unsigned int o) {
        return reinterpret_cast<volatile CPU::Reg32 *>(Memory_Map::OTP_BASE)[o / sizeof(CPU::Reg32)];
    }
};

class OTP: private OTP_Common, private SiFive_OTP {};

__END_SYS

#endif
