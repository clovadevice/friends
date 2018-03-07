#ifndef _NTP8824_H
#define _NTP8824_H

typedef unsigned char cfg_u8;
typedef unsigned int cfg_u32;

    

typedef union {
    struct {
        cfg_u8 offset;
        cfg_u8 value;
    };
    struct {
        cfg_u8 command;
        cfg_u8 param;
    };
} cfg_reg;

typedef union {
    struct {
        cfg_u8 offset;
        cfg_u32 value;
    };
    struct {
        cfg_u8 command;
        cfg_u32 param;
    };
} cfg_reg_4byte;


#define CFG_META_SWITCH (255)
#define CFG_META_DELAY  (254)  
#define CFG_META_BURST  (253)

#define BUF_MAX 300

// Brown register
// 2018-1-17 kh_shin
// 20180108_9th_NTP8824_PBTL_1.536MHz_AD_PS61_DRC-3_-3_-3_1.5dBgain_45pi_768_3.8k_1_9k_g1_SDATAOUT
cfg_reg registers_brown_1[] ={
    { .command=0x00, .param=0x00 }, // % I2S / Slave
    { .command=0x01, .param=0x00 }, // % General Serial Audio Format, param=0x Justify=Left, Bit Order=MSB first, Bit Size=24 bit, BCK/WCK ratio=64
    { .command=0x78, .param=0x00 }, // % User mode Setting for 1.536MHz
    { .command=0x79, .param=0x06 }, // % 
    { .command=0x02, .param=0x04 }, // % MCLK 1.536MHz,  0x02, param=0x0x04 PLL user defined mode
    { .command=0x03, .param=0x36 }, // % Mixer 00 = -6.0 dB
    { .command=0x04, .param=0x36 }, // % Mixer 01 = -6.0 dB
    { .command=0x05, .param=0x00 }, // % Mixer 10 = -150dB
    { .command=0x06, .param=0x00 }, // % Mixer 11 = -150dB
    { .command=0x47, .param=0x01 }, // % PWM SWITCHING FREQ. 768kHz
    { .command=0x43, .param=0x00 }, // % AD mode // 0x02 D-BTL mode
    { .command=0x45, .param=0x00 }, // % AD mode // 0x03 D-BTL mode modulator
    { .command=0x3E, .param=0x08 }, // % PBTLB
    { .command=0x3F, .param=0x08 }, // % PBTL
    { .command=0x76, .param=0x3F }, // % SDATA OUT --> Monitor 1
    
    // Speaker_table_PostBQ[] = 
    { .command=0x7E, .param=0x03 }  // % 4byte download page setting for ch1/ch2 together
};
	
cfg_reg_4byte registers_brown_2[] ={
	{ .command=0x00, .param=0x107F6DB5 },
    { .command=0x01, .param=0x11FF6DB5 },
    { .command=0x02, .param=0x107F6DB5 },
    { .command=0x03, .param=0x117F6D50 },
    { .command=0x04, .param=0x10FEDC33 },   /* 01.HPF        fc=75Hz,	 Gain=1.0dB, 	 Q=1.1 */ 

    { .command=0x05, .param=0x11006CBE },
    { .command=0x06, .param=0x11FEFB7D },
    { .command=0x07, .param=0x107D2132 },
    { .command=0x08, .param=0x117EFB7D },
    { .command=0x09, .param=0x10FDFAAD },   /* 02.PeakingEQ  fc=115Hz,	 Gain=5.3dB, 	 Q=0.7 */

    { .command=0x0A, .param=0x107EFC6D },
    { .command=0x0B, .param=0x11FD6CFF },
    { .command=0x0C, .param=0x107BFF9E },
    { .command=0x0D, .param=0x117D6CFF },
    { .command=0x0E, .param=0x10FAFC0B },	/* 03.PeakingEQ  fc=350Hz,	 Gain=-4.5dB, 	 Q=1.5 */

    { .command=0x0F, .param=0x11010C90 },
    { .command=0x10, .param=0x11FBF202 },
    { .command=0x11, .param=0x1076BA3B },
    { .command=0x12, .param=0x117BF202 },
    { .command=0x13, .param=0x10F8D35A },	/* 04.PeakingEQ  fc=930Hz,	 Gain=4dB, 	 Q=1.7 */ 

    { .command=0x14, .param=0x107CAC69 },
    { .command=0x15, .param=0x11F2EA13 },
    { .command=0x16, .param=0x106C8BD9 },
    { .command=0x17, .param=0x1172EA13 },
    { .command=0x18, .param=0x10E93841 },   /* 05.PeakingEQ  fc=1800Hz,	 Gain=-3dB, 	 Q=1.5 */ 

    { .command=0x19, .param=0x1075B478 },
    { .command=0x1A, .param=0x11EB6CC6 },
    { .command=0x1B, .param=0x106897F4 },
    { .command=0x1C, .param=0x116B6CC6 },
    { .command=0x1D, .param=0x10DE4C6D },	/* 06.PeakingEQ  fc=2700Hz,	 Gain=-8.2dB, 	 Q=2 */ 

    { .command=0x1E, .param=0x111E6E10 },
    { .command=0x1F, .param=0x12886F3D },
    { .command=0x20, .param=0x10744AE2 },
    { .command=0x21, .param=0x1157F47C },
    { .command=0x22, .param=0x10BF5305 },   /* 07.HighShelf  fc=3800Hz,	 Gain=2dB, 	 Q=0.9 */ 

    { .command=0x23, .param=0x11006D08 },
    { .command=0x24, .param=0x11F9BCEC },
    { .command=0x25, .param=0x10761C46 },
    { .command=0x26, .param=0x1179BCEC },
    { .command=0x27, .param=0x10F6F657 },   /* 08.PeakingEQ  fc=1800Hz,	 Gain=1.5dB, 	 Q=3 */ 

    { .command=0x28, .param=0x11065911 },
    { .command=0x29, .param=0x11DC8239 },
    { .command=0x2A, .param=0x104509BA },
    { .command=0x2B, .param=0x115C8239 },
    { .command=0x2C, .param=0x10D1BBDB },   /* 09.PeakingEQ  fc=5000Hz,	 Gain=3.8dB, 	 Q=1.3 */ 

    { .command=0x2D, .param=0x110CBFE2 },
    { .command=0x2E, .param=0x11B9B8DC },
    { .command=0x2F, .param=0x102121FE },
    { .command=0x30, .param=0x112396F1 },
    { .command=0x31, .param=0x108E5DEC },	 /* 10.HighShelf  fc=9028Hz,	 Gain=1dB, 	 Q=1 */

    { .command=0x5C, .param=0x107FD886 },
    { .command=0x5D, .param=0x11FF3800 },
    { .command=0x5E, .param=0x107EA79B },
    { .command=0x5F, .param=0x117F3800 },
    { .command=0x60, .param=0x10FE8021 },   /* 11.PeakingEQ  fc=240Hz,	 Gain=-2dB, 	 Q=3 */ 

    { .command=0x61, .param=0x09089F90 },
    { .command=0x62, .param=0x20000000 },
    { .command=0x63, .param=0x09889F90 },
    { .command=0x64, .param=0x117EEDDB },
    { .command=0x65, .param=0x10FDDDF9 },    /* 12.PeakingEQ  fc=90Hz,	 Gain=-5dB, 	 Q=1 */ 

    // [Dynamic Range Compressor]
    // BandMode=1
    // CASMode=0
    // DRC_Order=1
    // DRC_Fc=4000
    // DRC_Fc_Sub=232
    // DRC_Fc_Bar=19990
    // DRC_Fc_Sub_Bar=19990
    // [LDRC1]
    { .command=0x32, .param=0x0D6E41D5 },
    { .command=0x33, .param=0x0D6E41D5 },
    { .command=0x34, .param=0x20000000 },
    { .command=0x35, .param=0x10446F8A },
    { .command=0x36, .param=0x20000000 },
    // [LDRC2]
    { .command=0x37, .param=0x0A7726DE },
    { .command=0x38, .param=0x0B7726DE },
    { .command=0x39, .param=0x0A7726DE },
    { .command=0x3A, .param=0x115AF191 },
    { .command=0x3B, .param=0x10C55590 },
    // [HDRC1]
    { .command=0x3C, .param=0x106237C4 }, // % High 4000Hz
    { .command=0x3D, .param=0x10E237C4 },
    { .command=0x3E, .param=0x20000000 },
    { .command=0x3F, .param=0x10446F8A },
    { .command=0x40, .param=0x20000000 },
    // [HDRC2]
    { .command=0x41, .param=0x105ECE2C },
    { .command=0x42, .param=0x11DECE2C },
    { .command=0x43, .param=0x105ECE2C },
    { .command=0x44, .param=0x115AF191 },
    { .command=0x45, .param=0x10C55590 },
    // [SubDRC1]
    { .command=0x46, .param=0x0976E92A }, // % Sub DRC 232Hz
    { .command=0x47, .param=0x0976E92A },
    { .command=0x48, .param=0x20000000 },
    { .command=0x49, .param=0x107C245B },
    { .command=0x4A, .param=0x20000000 },
    // [SubDRC2]
    { .command=0x4B, .param=0x11000000 },
    { .command=0x4C, .param=0x20000000 },
    { .command=0x4D, .param=0x20000000 },
    { .command=0x4E, .param=0x20000000 },
    { .command=0x4F, .param=0x20000000 }
};
	
cfg_reg registers_brown_3[] ={
	{ .command=0x7E, .param=0x00 }, // 1 byte register page setting
	{ .command=0x7E, .param=0x08 }  // 4 byte APEQ register page setting
};
	
cfg_reg_4byte registers_brown_4[]={
    { .command=0x00, .param=0x104B5918 },
    { .command=0x01, .param=0x105765AC },
    { .command=0x02, .param=0x1021866B },
    { .command=0x03, .param=0x102F14D6 },
    { .command=0x04, .param=0x11212478 },
    { .command=0x05, .param=0x107D11D0 },
    { .command=0x06, .param=0x11000000 },
    { .command=0x07, .param=0x11000000 },
    { .command=0x08, .param=0x11000000 },
    { .command=0x09, .param=0x1F7FFFFF },
    { .command=0x0A, .param=0x1F7FFFFF },
    { .command=0x0B, .param=0x1F7FFFFF },
    { .command=0x0C, .param=0x0F0491E0 },
    { .command=0x0D, .param=0x0E4106B8 },
    { .command=0x0E, .param=0x1015BB90 },
    { .command=0x0F, .param=0x0F6CA27C },
    { .command=0x10, .param=0x0ED29BA0 },
    { .command=0x11, .param=0x0A3DB800 },
    { .command=0x12, .param=0x20000000 },
    { .command=0x13, .param=0x20000000 },
    { .command=0x14, .param=0x20000000 },
    { .command=0x15, .param=0x11800000 },
    { .command=0x16, .param=0x11800000 },
    { .command=0x17, .param=0x11800000 },
    { .command=0x1F, .param=0x137E4000 },
    { .command=0x20, .param=0x11000000 },
    { .command=0x21, .param=0x11000000 },
    { .command=0x22, .param=0x1134C000 },
    { .command=0x23, .param=0x11000000 },
    { .command=0x24, .param=0x127EC000 }
};
	
cfg_reg registers_brown_5[] = {
    { .command=0x7E, .param=0x00 }, // 1 byte register page setting
    { .command=0x19, .param=0x00 }, // % APEQ Path Enable
    
    { .command=0x0E, .param=0x15 }, // % CH1 Biquad Config BQ01~Bq03
    { .command=0x0F, .param=0x15 }, // % CH2 Biquad Config BQ01~Bq03
    { .command=0x10, .param=0x07 }, // % CH1 Biquad Config BQ04~Bq06
    { .command=0x11, .param=0x07 }, // % CH2 Biquad Config BQ04~Bq06
    { .command=0x12, .param=0x55 }, // % CH1 Biquad Config BQ07~Bq10
    { .command=0x13, .param=0x55 }, // % CH2 Biquad Config BQ07~Bq10
    { .command=0x14, .param=0x09 }, // % CH1 Biquad Config BQ11~Bq12 PEQ
    { .command=0x15, .param=0x09 }, // % CH2 Biquad Config BQ11~Bq12 APEQ
    
    { .command=0x1A, .param=0x00 }, // % APEQ filter control 2 for BQ7
    { .command=0x1B, .param=0x00 }, // % APEQ filter control 2 for BQ8
    { .command=0x1C, .param=0x00 }, // % APEQ filter control 2 for BQ9
    { .command=0x1D, .param=0x00 }, // % APEQ filter control 2 for BQ10
    { .command=0x1E, .param=0x00 }, // % APEQ filter control 2 for BQ11
    { .command=0x1F, .param=0x00 }, // % APEQ filter control 2 for BQ12
    
    // DRC 
    { .command=0x20, .param=0xC6 }, // DRC threshold for Low band = -3 dB
    { .command=0x21, .param=0x13 }, // DRC Low band Attack Time=30 msec, Release Time=1.0 sec
    { .command=0x22, .param=0xC6 }, // DRC threshold for High band = -3 dB
    { .command=0x23, .param=0x11 }, // DRC High band Attack Time = 8 msec, Release Time = 1.0 sec
    { .command=0x2A, .param=0xC6 }, // DRC threshold for Sub band = -3 dB
    { .command=0x2B, .param=0x1A }, // DRC Sub band Attack Time = 7 msec, Release Time = 1.0 sec
    { .command=0x26, .param=0xC6 }, // DRC threshold for Post band = -3 dB
    { .command=0x27, .param=0x02 }, // DRC Post band Attack Time = 15 msec, Release Time = 0.5 sec
    { .command=0x29, .param=0x11 }, // % DRC Control 2Band, Clip
    { .command=0x2C, .param=0x01 }, // % SubBand Mode
    
    { .command=0x16, .param=0x00 }, // % Master Volume Fine Control
    { .command=0x17, .param=0xA2 }, // % CH1 Volume , -1dB-->-0.5dB-->1.5dB
    { .command=0x18, .param=0xA2 }, // % CH2 Volume , -1dB-->-0.5dB-->1.5dB
    { .command=0x3C, .param=0x61 }, // % Prescaler Value
    { .command=0x41, .param=0x12 }, // % Minimum Pulse Width = 4
    // Sound ON
    { .command=0x35, .param=0x04 }, // PWM Mask ON
    { .command=0x34, .param=0x00 }, // PWM Switching ON
    { .command=0x33, .param=0x00 }, // Soft-mute OFF
    { .command=0x0C, .param=0xFF }  // Master Volume 0dB
};

//wjhyun 20171024
cfg_reg registers_mute[] ={
{ .command=0x0C, .param=0x00 } // Master Volume 0dB	
};

cfg_reg registers_unmute[] ={
{ .command=0x0C, .param=0xFF } // Master Volume 0dB	
};

#endif
