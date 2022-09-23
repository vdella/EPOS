#ifndef __riscv_sifive_u_otp_h
#define __riscv_sifive_u_otp_h

#include <architecture/cpu.h>
#include <system/memory_map.h>

__BEGIN_SYS

#define PA_RESET_VAL		    0x00
#define PAS_RESET_VAL		    0x00
#define PAIO_RESET_VAL		    0x00
#define PDIN_RESET_VAL		    0x00
#define PTM_RESET_VAL		    0x00

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

class OTP
{
private:
    typedef CPU:Reg32 Reg32;

public:
    static const unsigned int BYTES_PER_FUSE = Traits<OTP>::BYTES_PER_FUSE;
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
    OTP() {}

    static int sifive_otp_read(int offset, void *buf, int size)
    {
        // Check if offset and size are multiple of BYTES_PER_FUSE.
        if ((size % BYTES_PER_FUSE) || (offset % BYTES_PER_FUSE)) {
            printf("%s: size and offset must be multiple of 4.\n", __func__);
            return -EINVAL;
        }

        int fuse_id = offset / BYTES_PER_FUSE;
        int fuse_count = size / BYTES_PER_FUSE;

        // Check bounds.
        if (offset < 0 || size < 0)
            return -EINVAL;
        if (fuse_id >= plat->total_fuses)
            return -EINVAL;
        if ((fuse_id + fuse_count) > plat->total_fuses)
            return -EINVAL;

        int fuse_buf[fuse_count];

        // Init OTP.
        writel(PDSTB_DEEP_STANDBY_ENABLE, OTP::PDSTB);
        writel(PTRIM_ENABLE_INPUT, OTP::PTRIM);
        writel(PCE_ENABLE_INPUT, OTP::PCE);

        // Read all requested fuses.
        for (unsigned int i = 0; i < fuse_count; i++, fuse_id++) {
            writel(fuse_id, OTP::PA);

            // Cycle clock to read.
            writel(PCLK_ENABLE_VAL, OTP::PCLK);
            ndelay(TCD_DELAY * 1000);
            writel(PCLK_DISABLE_VAL, OTP::PCLK);
            ndelay(TKL_DELAY * 1000);

            // Read the value.
            fuse_buf[i] = readl(OTP::PDOUT);
        }

        // Shut down.
        writel(PCE_DISABLE_INPUT, OTP::PCE);
        writel(PTRIM_DISABLE_INPUT, OTP::PTRIM);
        writel(PDSTB_DEEP_STANDBY_DISABLE, OTP::PDSTB);

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
    static int sifive_otp_write(int offset, const void *buf, int size)
    {
        // Check if offset and size are multiple of BYTES_PER_FUSE.
        if ((size % BYTES_PER_FUSE) || (offset % BYTES_PER_FUSE)) {
            printf("%s: size and offset must be multiple of 4.\n",
                   __func__);
            return -EINVAL;
        }

        int fuse_id = offset / BYTES_PER_FUSE;
        int fuse_count = size / BYTES_PER_FUSE;
        Reg32 *write_buf = (Reg32 *) buf;
        Reg32 write_data;
        int i, pas, bit;

        // Check bounds.
        if (offset < 0 || size < 0)
            return -EINVAL;
        if (fuse_id >= plat->total_fuses)
            return -EINVAL;
        if ((fuse_id + fuse_count) > plat->total_fuses)
            return -EINVAL;

        // Init OTP.
        writel(PDSTB_DEEP_STANDBY_ENABLE, OTP::PDSTB);
        writel(PTRIM_ENABLE_INPUT, OTP::PTRIM);

        // Reset registers.
        writel(PCLK_DISABLE_VAL, OTP::PCLK);
        writel(PA_RESET_VAL, OTP::PA);
        writel(PAS_RESET_VAL, OTP::PAS);
        writel(PAIO_RESET_VAL, OTP::PAIO);
        writel(PDIN_RESET_VAL, OTP::PDIN);
        writel(PWE_WRITE_DISABLE, OTP::PWE);
        writel(PTM_FUSE_PROGRAM_VAL, OTP::PTM);
        ndelay(TMS_DELAY * 1000);

        writel(PCE_ENABLE_INPUT, OTP::PCE);
        writel(PPROG_ENABLE_INPUT, OTP::PPROG);

        // Write all requested fuses.
        for (i = 0; i < fuse_count; i++, fuse_id++) {
            writel(fuse_id, OTP::PA);
            Reg32 write_data = *(write_buf++);

            for (pas = 0; pas < 2; pas++) {
                writel(pas, OTP::PAS);

                for (bit = 0; bit < 32; bit++) {
                    writel(bit, OTP::PAIO);
                    writel(((write_data >> bit) & 1), OTP::PDIN);
                    ndelay(TASP_DELAY * 1000);

                    writel(PWE_WRITE_ENABLE, OTP::PWE);
                    udelay(TPW_DELAY);
                    writel(PWE_WRITE_DISABLE, OTP::PWE);
                    udelay(TPWI_DELAY);
                }
            }

            writel(PAS_RESET_VAL, OTP::PAS);
        }

        // Shut down.
        writel(PWE_WRITE_DISABLE, OTP::PWE);
        writel(PPROG_DISABLE_INPUT, OTP::PPROG);
        writel(PCE_DISABLE_INPUT, OTP::PCE);
        writel(PTM_RESET_VAL, OTP::PTM);

        writel(PTRIM_DISABLE_INPUT, OTP::PTRIM);
        writel(PDSTB_DEEP_STANDBY_DISABLE, OTP::PDSTB);

        return size;
    }
};

__END_SYS

#endif
