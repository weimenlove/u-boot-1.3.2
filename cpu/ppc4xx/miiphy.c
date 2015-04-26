/*-----------------------------------------------------------------------------+
  |
  |	  This source code has been made available to you by IBM on an AS-IS
  |	  basis.  Anyone receiving this source is licensed under IBM
  |	  copyrights to use it in any way he or she deems fit, including
  |	  copying it, modifying it, compiling it, and redistributing it either
  |	  with or without modifications.  No license under IBM patents or
  |	  patent applications is to be implied by the copyright license.
  |
  |	  Any user of this software should understand that IBM cannot provide
  |	  technical support for this software and will not be responsible for
  |	  any consequences resulting from the use of this software.
  |
  |	  Any person who transfers this source code or any derivative work
  |	  must include the IBM copyright notice, this paragraph, and the
  |	  preceding two paragraphs in the transferred software.
  |
  |	  COPYRIGHT   I B M   CORPORATION 1995
  |	  LICENSED MATERIAL  -	PROGRAM PROPERTY OF I B M
  +-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------+
  |
  |  File Name:	 miiphy.c
  |
  |  Function:	 This module has utilities for accessing the MII PHY through
  |	       the EMAC3 macro.
  |
  |  Author:	 Mark Wisner
  |
  +-----------------------------------------------------------------------------*/

#include <common.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <ppc_asm.tmpl>
#include <commproc.h>
#include <ppc4xx_enet.h>
#include <405_mal.h>
#include <miiphy.h>

#undef ET_DEBUG
/***********************************************************/
/* Dump out to the screen PHY regs			   */
/***********************************************************/

void miiphy_dump (char *devname, unsigned char addr)
{
	unsigned long i;
	unsigned short data;

	for (i = 0; i < 0x1A; i++) {
		if (miiphy_read (devname, addr, i, &data)) {
			printf ("read error for reg %lx\n", i);
			return;
		}
		printf ("Phy reg %lx ==> %4x\n", i, data);

		/* jump to the next set of regs */
		if (i == 0x07)
			i = 0x0f;

	}			/* end for loop */
}				/* end dump */

/***********************************************************/
/* (Re)start autonegotiation				   */
/***********************************************************/
int phy_setup_aneg (char *devname, unsigned char addr)
{
	u16 bmcr;

#if defined(CONFIG_PHY_DYNAMIC_ANEG)
	/*
	 * Set up advertisement based on capablilities reported by the PHY.
	 * This should work for both copper and fiber.
	 */
	u16 bmsr;
#if defined(CONFIG_PHY_GIGE)
	u16 exsr = 0x0000;
#endif

	miiphy_read (devname, addr, PHY_BMSR, &bmsr);

#if defined(CONFIG_PHY_GIGE)
	if (bmsr & PHY_BMSR_EXT_STAT)
		miiphy_read (devname, addr, PHY_EXSR, &exsr);

	if (exsr & (PHY_EXSR_1000XF | PHY_EXSR_1000XH)) {
		/* 1000BASE-X */
		u16 anar = 0x0000;

		if (exsr & PHY_EXSR_1000XF)
			anar |= PHY_X_ANLPAR_FD;

		if (exsr & PHY_EXSR_1000XH)
			anar |= PHY_X_ANLPAR_HD;

		miiphy_write (devname, addr, PHY_ANAR, anar);
	} else
#endif
	{
		u16 anar, btcr;

		miiphy_read (devname, addr, PHY_ANAR, &anar);
		anar &= ~(0x5000 | PHY_ANLPAR_T4 | PHY_ANLPAR_TXFD |
			  PHY_ANLPAR_TX | PHY_ANLPAR_10FD | PHY_ANLPAR_10);

		miiphy_read (devname, addr, PHY_1000BTCR, &btcr);
		btcr &= ~(0x00FF | PHY_1000BTCR_1000FD | PHY_1000BTCR_1000HD);

		if (bmsr & PHY_BMSR_100T4)
			anar |= PHY_ANLPAR_T4;

		if (bmsr & PHY_BMSR_100TXF)
			anar |= PHY_ANLPAR_TXFD;

		if (bmsr & PHY_BMSR_100TXH)
			anar |= PHY_ANLPAR_TX;

		if (bmsr & PHY_BMSR_10TF)
			anar |= PHY_ANLPAR_10FD;

		if (bmsr & PHY_BMSR_10TH)
			anar |= PHY_ANLPAR_10;

		miiphy_write (devname, addr, PHY_ANAR, anar);

#if defined(CONFIG_PHY_GIGE)
		if (exsr & PHY_EXSR_1000TF)
			btcr |= PHY_1000BTCR_1000FD;

		if (exsr & PHY_EXSR_1000TH)
			btcr |= PHY_1000BTCR_1000HD;

		miiphy_write (devname, addr, PHY_1000BTCR, btcr);
#endif
	}

#else /* defined(CONFIG_PHY_DYNAMIC_ANEG) */
	/*
	 * Set up standard advertisement
	 */
	u16 adv;

	miiphy_read (devname, addr, PHY_ANAR, &adv);
	adv |= (PHY_ANLPAR_ACK  | PHY_ANLPAR_TXFD | PHY_ANLPAR_TX |
		PHY_ANLPAR_10FD | PHY_ANLPAR_10);
	miiphy_write (devname, addr, PHY_ANAR, adv);

	miiphy_read (devname, addr, PHY_1000BTCR, &adv);
	adv |= (0x0300);
	miiphy_write (devname, addr, PHY_1000BTCR, adv);

#endif /* defined(CONFIG_PHY_DYNAMIC_ANEG) */

	/* Start/Restart aneg */
	miiphy_read (devname, addr, PHY_BMCR, &bmcr);
	bmcr |= (PHY_BMCR_AUTON | PHY_BMCR_RST_NEG);
	miiphy_write (devname, addr, PHY_BMCR, bmcr);

	return 0;
}

/***********************************************************/
/* read a phy reg and return the value with a rc	   */
/***********************************************************/
unsigned int miiphy_getemac_offset (void)
{
#if (defined(CONFIG_440) && !defined(CONFIG_440SP) && !defined(CONFIG_440SPE)) && defined(CONFIG_NET_MULTI)
	unsigned long zmii;
	unsigned long eoffset;

	/* Need to find out which mdi port we're using */
	zmii = in_be32((void *)ZMII_FER);

	if (zmii & (ZMII_FER_MDI << ZMII_FER_V (0)))
		/* using port 0 */
		eoffset = 0;

	else if (zmii & (ZMII_FER_MDI << ZMII_FER_V (1)))
		/* using port 1 */
		eoffset = 0x100;

	else if (zmii & (ZMII_FER_MDI << ZMII_FER_V (2)))
		/* using port 2 */
		eoffset = 0x400;

	else if (zmii & (ZMII_FER_MDI << ZMII_FER_V (3)))
		/* using port 3 */
		eoffset = 0x600;

	else {
		/* None of the mdi ports are enabled! */
		/* enable port 0 */
		zmii |= ZMII_FER_MDI << ZMII_FER_V (0);
		out_be32((void *)ZMII_FER, zmii);
		eoffset = 0;
		/* need to soft reset port 0 */
		zmii = in_be32((void *)EMAC_M0);
		zmii |= EMAC_M0_SRST;
		out_be32((void *)EMAC_M0, zmii);
	}

	return (eoffset);
#else

#if defined(CONFIG_NET_MULTI) && defined(CONFIG_405EX)
	unsigned long rgmii;
	int devnum = 1;

	rgmii = in_be32((void *)RGMII_FER);
	if (rgmii & (1 << (19 - devnum)))
		return 0x100;
#endif

	return 0;
#endif
}

int emac4xx_miiphy_read (char *devname, unsigned char addr, unsigned char reg,
			 unsigned short *value)
{
	unsigned long sta_reg;	/* STA scratch area */
	unsigned long i;
	unsigned long emac_reg;

	emac_reg = miiphy_getemac_offset ();
	/* see if it is ready for 1000 nsec */
	i = 0;

	/* see if it is ready for  sec */
	while ((in_be32((void *)EMAC_STACR + emac_reg) & EMAC_STACR_OC) ==
	       EMAC_STACR_OC_MASK) {
		udelay (7);
		if (i > 5) {
#ifdef ET_DEBUG
			sta_reg = in_be32((void *)EMAC_STACR + emac_reg);
			printf ("read : EMAC_STACR=0x%0x\n", sta_reg);	/* test-only */
			printf ("read err 1\n");
#endif
			return -1;
		}
		i++;
	}
	sta_reg = reg;		/* reg address */
	/* set clock (50Mhz) and read flags */
#if defined(CONFIG_440GX) || defined(CONFIG_440SPE) || \
    defined(CONFIG_440EPX) || defined(CONFIG_440GRX) || \
    defined(CONFIG_405EX)
#if defined(CONFIG_IBM_EMAC4_V4)	/* EMAC4 V4 changed bit setting */
	sta_reg = (sta_reg & ~EMAC_STACR_OP_MASK) | EMAC_STACR_READ;
#else
	sta_reg |= EMAC_STACR_READ;
#endif
#else
	sta_reg = (sta_reg | EMAC_STACR_READ) & ~EMAC_STACR_CLK_100MHZ;
#endif

#if defined(CONFIG_PHY_CLK_FREQ) && !defined(CONFIG_440GX) && \
    !defined(CONFIG_440SP) && !defined(CONFIG_440SPE) && \
    !defined(CONFIG_440EPX) && !defined(CONFIG_440GRX) && \
    !defined(CONFIG_405EX)
	sta_reg = sta_reg | CONFIG_PHY_CLK_FREQ;
#endif
	sta_reg = sta_reg | (addr << 5);	/* Phy address */
	sta_reg = sta_reg | EMAC_STACR_OC_MASK;	/* new IBM emac v4 */
	out_be32((void *)EMAC_STACR + emac_reg, sta_reg);
#ifdef ET_DEBUG
	printf ("a2: write: EMAC_STACR=0x%0x\n", sta_reg);	/* test-only */
#endif

	sta_reg = in_be32((void *)EMAC_STACR + emac_reg);
#ifdef ET_DEBUG
	printf ("a21: read : EMAC_STACR=0x%0x\n", sta_reg);	/* test-only */
#endif
	i = 0;
	while ((sta_reg & EMAC_STACR_OC) == EMAC_STACR_OC_MASK) {
		udelay (7);
		if (i > 5)
			return -1;

		i++;
		sta_reg = in_be32((void *)EMAC_STACR + emac_reg);
#ifdef ET_DEBUG
		printf ("a22: read : EMAC_STACR=0x%0x\n", sta_reg);	/* test-only */
#endif
	}
	if ((sta_reg & EMAC_STACR_PHYE) != 0)
		return -1;

	*value = *(short *)(&sta_reg);
	return 0;

}				/* phy_read */

/***********************************************************/
/* write a phy reg and return the value with a rc	    */
/***********************************************************/

int emac4xx_miiphy_write (char *devname, unsigned char addr, unsigned char reg,
			  unsigned short value)
{
	unsigned long sta_reg;	/* STA scratch area */
	unsigned long i;
	unsigned long emac_reg;

	emac_reg = miiphy_getemac_offset ();
	/* see if it is ready for 1000 nsec */
	i = 0;

	while ((in_be32((void *)EMAC_STACR + emac_reg) & EMAC_STACR_OC) ==
	       EMAC_STACR_OC_MASK) {
		if (i > 5)
			return -1;

		udelay (7);
		i++;
	}
	sta_reg = 0;
	sta_reg = reg;		/* reg address */
	/* set clock (50Mhz) and read flags */
#if defined(CONFIG_440GX) || defined(CONFIG_440SPE) || \
    defined(CONFIG_440EPX) || defined(CONFIG_440GRX) || \
    defined(CONFIG_405EX)
#if defined(CONFIG_IBM_EMAC4_V4)	/* EMAC4 V4 changed bit setting */
	sta_reg = (sta_reg & ~EMAC_STACR_OP_MASK) | EMAC_STACR_WRITE;
#else
	sta_reg |= EMAC_STACR_WRITE;
#endif
#else
	sta_reg = (sta_reg | EMAC_STACR_WRITE) & ~EMAC_STACR_CLK_100MHZ;
#endif

#if defined(CONFIG_PHY_CLK_FREQ) && !defined(CONFIG_440GX) && \
    !defined(CONFIG_440SP) && !defined(CONFIG_440SPE) && \
    !defined(CONFIG_440EPX) && !defined(CONFIG_440GRX) && \
    !defined(CONFIG_405EX)
	sta_reg = sta_reg | CONFIG_PHY_CLK_FREQ;	/* Set clock frequency (PLB freq. dependend) */
#endif
	sta_reg = sta_reg | ((unsigned long)addr << 5);	/* Phy address */
	sta_reg = sta_reg | EMAC_STACR_OC_MASK;	/* new IBM emac v4 */
	memcpy (&sta_reg, &value, 2);	/* put in data */

	out_be32((void *)EMAC_STACR + emac_reg, sta_reg);

	/* wait for completion */
	i = 0;
	sta_reg = in_be32((void *)EMAC_STACR + emac_reg);
#ifdef ET_DEBUG
	printf ("a31: read : EMAC_STACR=0x%0x\n", sta_reg);	/* test-only */
#endif
	while ((sta_reg & EMAC_STACR_OC) == EMAC_STACR_OC_MASK) {
		udelay (7);
		if (i > 5)
			return -1;

		i++;
		sta_reg = in_be32((void *)EMAC_STACR + emac_reg);
#ifdef ET_DEBUG
		printf ("a32: read : EMAC_STACR=0x%0x\n", sta_reg);	/* test-only */
#endif
	}

	if ((sta_reg & EMAC_STACR_PHYE) != 0)
		return -1;

	return 0;

} /* phy_write */
