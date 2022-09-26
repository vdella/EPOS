#ifndef __riscv_sifive_u_otp_h
#define __riscv_sifive_u_otp_h

#include <architecture/cpu.h>
#include <machine/otp.h>
#include <machine/io.h>
#include <system/memory_map.h>

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

class OTP: private OTP_Common
{
private:
    typedef CPU::Reg32 Reg32;

public:
    static const unsigned int BYTES_PER_FUSE = 4;
    static const unsigned int total_fuses = 4;
//    static const unsigned int TPW_DELAY      = Traits<OTP>::TPW_DELAY;
//    static const unsigned int TPWI_DELAY     = Traits<OTP>::TPWI_DELAY;
//    static const unsigned int TASP_DELAY     = Traits<OTP>::TASP_DELAY;
//    static const unsigned int TCD_DELAY      = Traits<OTP>::TCD_DELAY;
//    static const unsigned int TKL_DELAY      = Traits<OTP>::TKL_DELAY;
//    static const unsigned int TMS_DELAY      = Traits<OTP>::TMS_DELAY;

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
    OTP() {}

    static int otp_read(int offset, void *buf, int size)
    {
        // Check if offset and size are multiple of BYTES_PER_FUSE.
        if ((size % BYTES_PER_FUSE) || (offset % BYTES_PER_FUSE)) {
            return -1;
        }

        unsigned int fuse_id = offset / BYTES_PER_FUSE;
        unsigned int fuse_count = size / BYTES_PER_FUSE;

        // Check bounds.
        if (offset < 0 || size < 0)
            return -1;
        if (fuse_id >= total_fuses)
            return -1;
        if ((fuse_id + fuse_count) > total_fuses)
            return -1;

        int fuse_buf[fuse_count];

        // Init OTP.
        writel(PDSTB_DEEP_STANDBY_ENABLE, PDSTB);
        writel(PTRIM_ENABLE_INPUT, PTRIM);
        writel(PCE_ENABLE_INPUT, PCE);

        // Read all requested fuses.
        for (unsigned int i = 0; i < fuse_count; i++, fuse_id++) {
            writel(fuse_id, PA);

            // Cycle clock to read.
            writel(PCLK_ENABLE_VAL, PCLK);
            writel(PCLK_DISABLE_VAL, PCLK);
            
            // Read the value.
            fuse_buf[i] = readl(PDOUT);
        }

        // Shut down.
        writel(PCE_DISABLE_INPUT, PCE);
        writel(PTRIM_DISABLE_INPUT, PTRIM);
        writel(PDSTB_DEEP_STANDBY_DISABLE, PDSTB);

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
    static int otp_write(int offset, const void *buf, int size)
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
        writel(PDSTB_DEEP_STANDBY_ENABLE, PDSTB);
        writel(PTRIM_ENABLE_INPUT, PTRIM);

        // Reset registers.
        writel(PCLK_DISABLE_VAL, PCLK);
        writel(PA_RESET_VAL, PA);
        writel(PAS_RESET_VAL, PAS);
        writel(PAIO_RESET_VAL, PAIO);
        writel(PDIN_RESET_VAL, PDIN);
        writel(PWE_WRITE_DISABLE, PWE);
        writel(PTM_FUSE_PROGRAM_VAL, PTM);

        writel(PCE_ENABLE_INPUT, PCE);
        writel(PPROG_ENABLE_INPUT, PPROG);

        // Write all requested fuses.
        for (unsigned int i = 0; i < fuse_count; i++, fuse_id++) {
            writel(fuse_id, PA);
            Reg32 write_data = *(write_buf++);

            for (pas = 0; pas < 2; pas++) {
                writel(pas, PAS);

                for (bit = 0; bit < 32; bit++) {
                    writel(bit, PAIO);
                    writel(((write_data >> bit) & 1), PDIN);

                    writel(PWE_WRITE_ENABLE, PWE);
                    writel(PWE_WRITE_DISABLE, PWE);
                }
            }

            writel(PAS_RESET_VAL, PAS);
        }

        // Shut down.
        writel(PWE_WRITE_DISABLE, PWE);
        writel(PPROG_DISABLE_INPUT, PPROG);
        writel(PCE_DISABLE_INPUT, PCE);
        writel(PTM_RESET_VAL, PTM);

        writel(PTRIM_DISABLE_INPUT, PTRIM);
        writel(PDSTB_DEEP_STANDBY_DISABLE, PDSTB);

        return size;
    }
};

__END_SYS

#endif
