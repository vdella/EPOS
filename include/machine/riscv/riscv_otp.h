#ifndef __riscv_sifive_u_otp_h
#define __riscv_sifive_u_otp_h

#include <architecture/cpu.h>
#include <machine/otp.h>
#include <machine/io.h>
#include <system/memory_map.h>
#include <time.h>

__BEGIN_SYS

#define PA_RESET_VAL 0x00
#define PAS_RESET_VAL 0x00
#define PAIO_RESET_VAL 0x00
#define PDIN_RESET_VAL 0x00
#define PTM_RESET_VAL 0x00

#define BIT(nr) (1UL << (nr))

#define PCLK_ENABLE_VAL BIT(0)
#define PCLK_DISABLE_VAL 0x00

#define PWE_WRITE_ENABLE BIT(0)
#define PWE_WRITE_DISABLE 0x00

#define PTM_FUSE_PROGRAM_VAL BIT(1)

#define PCE_ENABLE_INPUT BIT(0)
#define PCE_DISABLE_INPUT 0x00

#define PPROG_ENABLE_INPUT BIT(0)
#define PPROG_DISABLE_INPUT 0x00

#define PTRIM_ENABLE_INPUT BIT(0)
#define PTRIM_DISABLE_INPUT 0x00

#define PDSTB_DEEP_STANDBY_ENABLE BIT(0)
#define PDSTB_DEEP_STANDBY_DISABLE 0x00

#define EINVAL 0

class SiFive_OTP
{
private:
    typedef CPU::Reg32 Reg32;

public:
    static const unsigned int BYTES_PER_FUSE = Traits<OTP>::BYTES_PER_FUSE;
    static const unsigned int TOTAL_FUSES = Traits<OTP>::TOTAL_FUSES;

    static const unsigned int TPW_DELAY = Traits<OTP>::TPW_DELAY;
    static const unsigned int TPWI_DELAY = Traits<OTP>::TPWI_DELAY;
    static const unsigned int TASP_DELAY = Traits<OTP>::TASP_DELAY;
    static const unsigned int TCD_DELAY = Traits<OTP>::TCD_DELAY;
    static const unsigned int TKL_DELAY = Traits<OTP>::TKL_DELAY;
    static const unsigned int TMS_DELAY = Traits<OTP>::TMS_DELAY;

    // OTP registers offsets from OTP_BASE
    enum
    {
        PA = 0x00,      // Address input
        PAIO = 0x04,    // Program address input
        PAS = 0x08,     // Program redundancy cell selection input
        PCE = 0x0C,     // OTP Macro enable input
        PCLK = 0x10,    // Clock input
        PDIN = 0x14,    // Write data input
        PDOUT = 0x18,   // Read data output
        PDSTB = 0x1C,   // Deep standby mode enable input (active low)
        PPROG = 0x20,   // Program mode enable input
        PTC = 0x24,     // Test column enable input
        PTM = 0x28,     // Test mode enable input
        PTM_REP = 0x2C, // Repair function test mode enable input
        PTR = 0x30,     // Test row enable input
        PTRIM = 0x34,   // Repair function enable input
        PWE = 0x38      // Write enable input (defines program cycle)
    };

public:
    SiFive_OTP() {}

    int read_shot(int offset, void *buf, int size)
    {

        // 252 * 4, write_buffer, (3840 - 252) * 4

        // offset = 252 * 4 = 1008
        // size = 14352

        // Check if offset and size are multiple of BYTES_PER_FUSE.
        if ((size % BYTES_PER_FUSE) || (offset % BYTES_PER_FUSE))
        {
            return EINVAL + 1;
        }

        unsigned int fuse_id = offset;  // 0
        unsigned int fuse_count = size; // 3840

        db<OTP>(WRN) << "Started reading OTP!" << endl;
        db<OTP>(WRN) << "Offset = " << offset << endl;
        db<OTP>(WRN) << "Size = " << size << endl;

        //  Check bounds.
        if (offset < 0 || size < 0)
            return EINVAL + 1;
        if (fuse_id >= TOTAL_FUSES)
            return EINVAL + 2;
        if ((fuse_id + fuse_count) > TOTAL_FUSES)
            return EINVAL + 3;

        int fuse_buf[fuse_count];

        db<OTP>(WRN) << "Gathering registers!" << endl;

        // Init OTP.
        IO::writel(PDSTB_DEEP_STANDBY_ENABLE, (volatile void*) SiFive_OTP::PDSTB);
        IO::writel(PTRIM_ENABLE_INPUT,  (volatile void*) SiFive_OTP::PTRIM);
        IO::writel(PCE_ENABLE_INPUT, (volatile void*) SiFive_OTP::PCE);

        db<OTP>(WRN) << "PDSTB REG = " << reg(SiFive_OTP::PDSTB) << endl;
        db<OTP>(WRN) << "PTRIM REG = " << reg(SiFive_OTP::PTRIM) << endl;
        db<OTP>(WRN) << "PCE REG = " << reg(SiFive_OTP::PCE) << endl;

        // Read all requested fuses.
        for (unsigned int i = 0; i < fuse_count; i++, fuse_id++)
        {
            IO::writel(fuse_id, (volatile void*) SiFive_OTP::PA);
            db<OTP>(WRN) << "PA REG = " << reg(SiFive_OTP::PA) << endl;

            /* cycle clock to read */
            IO::writel(PCLK_ENABLE_VAL, (volatile void*) SiFive_OTP::PCLK);
            db<OTP>(WRN) << "PCLK REG = " << reg(SiFive_OTP::PCLK) << endl;

            Delay tcd(TCD_DELAY);

            IO::writel(PCLK_DISABLE_VAL, (volatile void*) SiFive_OTP::PCLK);
            db<OTP>(WRN) << "PCLK REG = " << reg(SiFive_OTP::PCLK) << endl;

            Delay tkl(TKL_DELAY);

            /* read the value */
            db<OTP>(WRN) << "PDOUT REG = " << reg(SiFive_OTP::PDOUT) << endl;
            fuse_buf[i] = IO::readl((volatile void*) SiFive_OTP::PDOUT);
        }

        db<OTP>(WRN) << "PCE REG = " << reg(SiFive_OTP::PCE) << endl;
        db<OTP>(WRN) << "PTRIM REG = " << reg(SiFive_OTP::PTRIM) << endl;
        db<OTP>(WRN) << "PDSTB REG = " << reg(SiFive_OTP::PDSTB) << endl;

        // Shut down.
        IO::writel(PCE_DISABLE_INPUT, (volatile void*) SiFive_OTP::PCE);
        IO::writel(PTRIM_DISABLE_INPUT, (volatile void*) SiFive_OTP::PTRIM);
        IO::writel(PDSTB_DEEP_STANDBY_DISABLE, (volatile void*) SiFive_OTP::PDSTB);

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
        db<OTP>(WRN) << "Offset = " << offset << endl;
        db<OTP>(WRN) << "size = " << size << endl;

        // Check if offset and size are multiple of BYTES_PER_FUSE.
        if ((size % BYTES_PER_FUSE) || (offset % BYTES_PER_FUSE))
        {
            return EINVAL + 5;
        }

        unsigned int fuse_id = offset;
        unsigned int fuse_count = size;

        Reg32 *write_buf = (Reg32 *)buf;
        int pas, bit;

        // Check bounds.
        if (offset < 0 || size < 0)
            return EINVAL + 6;
        if (fuse_id >= TOTAL_FUSES)
            return EINVAL + 7;
        if ((fuse_id + fuse_count) > TOTAL_FUSES)
            return EINVAL + 8;

        // Init OTP.
        IO::writel(PDSTB_DEEP_STANDBY_ENABLE, (volatile void*) SiFive_OTP::PDSTB);
        IO::writel(PTRIM_ENABLE_INPUT, (volatile void*) SiFive_OTP::PTRIM);

        IO::writel(PCLK_DISABLE_VAL, (volatile void*) SiFive_OTP::PCLK);
        IO::writel(PA_RESET_VAL, (volatile void*) SiFive_OTP::PA);
        IO::writel(PAS_RESET_VAL, (volatile void*) SiFive_OTP::PAS);
        IO::writel(PAIO_RESET_VAL, (volatile void*) SiFive_OTP::PAIO);
        IO::writel(PDIN_RESET_VAL, (volatile void*) SiFive_OTP::PDIN);
        IO::writel(PWE_WRITE_DISABLE, (volatile void*) SiFive_OTP::PWE);
        IO::writel(PTM_FUSE_PROGRAM_VAL, (volatile void*) SiFive_OTP::PTM);

        Delay tms(TMS_DELAY);

        IO::writel(PCE_ENABLE_INPUT, (volatile void*) SiFive_OTP::PCE);
        IO::writel(PPROG_ENABLE_INPUT, (volatile void*) SiFive_OTP::PPROG);

        // Write all requested fuses.
        for (unsigned int i = 0; i < fuse_count; i++, fuse_id++)
        {
            IO::writel(fuse_id, (volatile void*) SiFive_OTP::PA);
            Reg32 write_data = *(write_buf++);

            for (pas = 0; pas < 2; pas++)
            {
                IO::writel(pas, (volatile void*) SiFive_OTP::PAS);

                for (bit = 0; bit < 32; bit++)
                {
                    IO::writel(bit, (volatile void*) SiFive_OTP::PAIO);
                    IO::writel((write_data >> bit) & 1, (volatile void*) SiFive_OTP::PDIN);

                    Delay tasp(TASP_DELAY);

                    IO::writel(PWE_WRITE_ENABLE, (volatile void*) SiFive_OTP::PWE);

                    // EPOS Timer
                    Delay tpw(TPW_DELAY);

                    IO::writel(PWE_WRITE_DISABLE, (volatile void*) SiFive_OTP::PWE);

                    // EPOS Timer
                    Delay tpwi(TPWI_DELAY);
                }
            }

            IO::writel(PAS_RESET_VAL, (volatile void*) SiFive_OTP::PAS);
        }

        // Shut down.
        IO::writel(PWE_WRITE_DISABLE, (volatile void*) SiFive_OTP::PWE);
        IO::writel(PPROG_DISABLE_INPUT, (volatile void*) SiFive_OTP::PPROG);
        IO::writel(PCE_DISABLE_INPUT, (volatile void*) SiFive_OTP::PCE);
        IO::writel(PTM_RESET_VAL, (volatile void*) SiFive_OTP::PTM);
        IO::writel(PTRIM_DISABLE_INPUT, (volatile void*) SiFive_OTP::PTRIM);
        IO::writel(PDSTB_DEEP_STANDBY_DISABLE, (volatile void*) SiFive_OTP::PDSTB);
        
        return size;
    }

private:
    static volatile CPU::Reg32 &reg(unsigned int o)
    {
        return reinterpret_cast<volatile CPU::Reg32 *>[o / sizeof(CPU::Reg32)];
    }
};

class OTP : private OTP_Common, private SiFive_OTP
{

public:
    void init() {};
};

__END_SYS

#endif
