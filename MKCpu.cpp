/*
 *--------------------------------------------------------------------
 * Project:    VM65 - Virtual Machine/CPU emulator programming
 *                   framework.  
 *
 * File:   		MKCpu.cpp
 *
 * Purpose: 		Implementation of MKCpu class.
 *             MKCpu class implements the op-codes and other
 *             supporting functions of a virtual CPU. This one in
 *             particular emulates a real world microprocessor from
 *             1970-s, the venerable MOS-6502.
 *
 * Date:     	8/25/2016
 *
 * Copyright:  (C) by Marek Karcz 2016. All rights reserved.
 *
 * Contact:    makarcz@yahoo.com
 *
 * License Agreement and Warranty:

   This software is provided with No Warranty.
   I (Marek Karcz) will not be held responsible for any damage to
   computer systems, data or user's health resulting from use.
   Please proceed responsibly and exercise common sense.
   This software is provided in hope that it will be useful.
   It is free of charge for non-commercial and educational use.
   Distribution of this software in non-commercial and educational
   derivative work is permitted under condition that original
   copyright notices and comments are preserved. Some 3-rd party work
   included with this project may require separate application for
   permission from their respective authors/copyright owners.

 *--------------------------------------------------------------------
 */
#include <string.h>
#include "MKCpu.h"
#include "MKGenException.h"

Qrack::QInterfaceEngine qUnitEngine = Qrack::QINTERFACE_QUNIT;
#if ENABLE_OPENCL
Qrack::QInterfaceEngine qUnitSubEngine1 = Qrack::QINTERFACE_OPENCL;
#else
Qrack::QInterfaceEngine qUnitSubEngine1 = Qrack::QINTERFACE_CPU;
#endif

Qrack::QInterfacePtr qReg = NULL;

namespace MKBasic {

/*
 *--------------------------------------------------------------------
 * Method:
 * Purpose:
 * Arguments:
 * Returns:
 *--------------------------------------------------------------------
 */

/*
 *--------------------------------------------------------------------
 * Method:		MKCpu()
 * Purpose:		Default constructor.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
MKCpu::MKCpu()
{
	InitCpu();
}

/*
 *--------------------------------------------------------------------
 * Method:		MKCpu()
 * Purpose:		Custom constructor.
 * Arguments:	pmem - pointer to Memory object.
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
MKCpu::MKCpu(Memory *pmem)
{
	mpMem = pmem;
	InitCpu();
}

/*
 *--------------------------------------------------------------------
 * Method:		InitCpu()
 * Purpose:		Initialize internal variables and flags.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
	ADDRMODE_IMM = 0,
	ADDRMODE_ABS,
	ADDRMODE_ZP,
	ADDRMODE_IMP,
	ADDRMODE_IND,
	ADDRMODE_ABX,
	ADDRMODE_ABY,
	ADDRMODE_ZPX,
	ADDRMODE_ZPY,
	ADDRMODE_IZX,
	ADDRMODE_IZY,
	ADDRMODE_REL,
	ADDRMODE_ACC, 
 */
void MKCpu::InitCpu()
{
	string saArgFmtTbl[] = {
		"#$%02x",
		"$%04x",
		"$%02x",
		"     ",
		"($%04x)",
		//"$%04x,A",
		"$%04x,X",
		"$%04x,Y",
		"$%02x,X",
		"$%02x,Y",
		"($%02x,X)",
		"($%02x),Y",
		"$%04x",
		"     ",
		"     "
	};
	int naAddrModesLen[] = {2, 3, 2, 1, 3, /*3,*/ 3, 3, 2, 2, 2, 2, 2, 1, 1};
	// Initialize instructions lengths table based on addressing modes
	// Initialize assembly argument formats table based on addressing modes
	for (int i=0; i<ADDRMODE_LENGTH; i++) {
		mAddrModesLen[i] = naAddrModesLen[i];
		mArgFmtTbl[i] = saArgFmtTbl[i];
	}
	// Initialize disassembly op-codes table/map.
	OpCodesMap myOpCodesMap {
		//opcode		opcode			addr. mode		time		symb.	pointer to exec. function	hex
		//------------------------------------------------------------------------------------------
		{OPCODE_BRK,		{OPCODE_BRK,		ADDRMODE_IMP,		7,		"BRK",	&MKCpu::OpCodeBrk 		/*00*/	}},
		{OPCODE_ORA_IZX,	{OPCODE_ORA_IZX,	ADDRMODE_IZX,		6,		"ORA",	&MKCpu::OpCodeOraIzx		/*01*/	}},
		{OPCODE_HAD_A,		{OPCODE_HAD_A,		ADDRMODE_IMP,		3,		"HAA",	&MKCpu::OpCodeHadA		/*02*/	}},
		{OPCODE_HAD_X,		{OPCODE_HAD_X,		ADDRMODE_IMP,		3,		"HAX",	&MKCpu::OpCodeHadX		/*03*/	}},
		{OPCODE_HAD_Y,		{OPCODE_HAD_Y,		ADDRMODE_IMP,		3,		"HAY",	&MKCpu::OpCodeHadY		/*04*/	}},
		{OPCODE_ORA_ZP,		{OPCODE_ORA_ZP,		ADDRMODE_ZP,		3,		"ORA", 	&MKCpu::OpCodeOraZp		/*05*/	}},
		{OPCODE_ASL_ZP,		{OPCODE_ASL_ZP,		ADDRMODE_ZP,		5,		"ASL",	&MKCpu::OpCodeAslZp		/*06*/	}},
		{OPCODE_ILL_07,		{OPCODE_ILL_07,		ADDRMODE_ZP,		5,		"SLO",	&MKCpu::OpCodeDud		/*07*/	}},
		{OPCODE_PHP,		{OPCODE_PHP,		ADDRMODE_IMP,		3,		"PHP",	&MKCpu::OpCodePhp		/*08*/	}},
		{OPCODE_ORA_IMM,	{OPCODE_ORA_IMM,	ADDRMODE_IMM,		2,		"ORA",	&MKCpu::OpCodeOraImm 		/*09*/	}},
		{OPCODE_ASL,		{OPCODE_ASL,		ADDRMODE_ACC,		2,		"ASL",	&MKCpu::OpCodeAslAcc		/*0a*/	}},
		{OPCODE_ILL_0B,		{OPCODE_ILL_0B,		ADDRMODE_IMM,		2,		"ANC",	&MKCpu::OpCodeDud 		/*0b*/	}},
		{OPCODE_PAX_Y,		{OPCODE_PAX_Y,		ADDRMODE_IMP,		3,		"PXY",	&MKCpu::OpCodeXY		/*0c*/	}},
		{OPCODE_ORA_ABS,	{OPCODE_ORA_ABS,	ADDRMODE_ABS,		4,		"ORA",	&MKCpu::OpCodeOraAbs 		/*0d*/	}},
		{OPCODE_ASL_ABS,	{OPCODE_ASL_ABS,	ADDRMODE_ABS,		6,		"ASL",	&MKCpu::OpCodeAslAbs 		/*0e*/	}},
		{OPCODE_SEN,		{OPCODE_SEN,		ADDRMODE_IMP,		6,		"SEN",	&MKCpu::OpCodeSen 		/*0f*/	}},
		{OPCODE_BPL_REL,	{OPCODE_BPL_REL,	ADDRMODE_REL,		2,		"BPL",	&MKCpu::OpCodeBplRel 		/*10*/	}},
		{OPCODE_ORA_IZY,	{OPCODE_ORA_IZY,	ADDRMODE_IZY,		5,		"ORA",	&MKCpu::OpCodeOraIzy 		/*11*/	}},
		{OPCODE_PAX_A,		{OPCODE_PAX_A,		ADDRMODE_IMP,		3,		"PXA",	&MKCpu::OpCodeXA 		/*12*/	}},
		{OPCODE_PAX_X,		{OPCODE_PAX_X,		ADDRMODE_IMP,		3,		"PXX",	&MKCpu::OpCodeXX 		/*13*/	}},
		{OPCODE_PAX_C,		{OPCODE_PAX_C,		ADDRMODE_IMP,		2,		"PXC",	&MKCpu::OpCodeXC		/*14*/	}},
		{OPCODE_ORA_ZPX,	{OPCODE_ORA_ZPX,	ADDRMODE_ZPX,		4,		"ORA",	&MKCpu::OpCodeOraZpx 		/*15*/	}},
		{OPCODE_ASL_ZPX,	{OPCODE_ASL_ZPX,	ADDRMODE_ZPX,		6,		"ASL",	&MKCpu::OpCodeAslZpx 		/*16*/	}},
		{OPCODE_HAD_C,		{OPCODE_HAD_C,		ADDRMODE_IMP,		4,		"HAC",	&MKCpu::OpCodeHac 		/*17*/	}},
		{OPCODE_CLC,		{OPCODE_CLC,		ADDRMODE_IMP,		2,		"CLC",	&MKCpu::OpCodeClc 		/*18*/	}},
		{OPCODE_ORA_ABY,	{OPCODE_ORA_ABY,	ADDRMODE_ABY,		4,		"ORA",	&MKCpu::OpCodeOraAby 		/*19*/	}},
		{OPCODE_PAY_A,		{OPCODE_PAY_A,		ADDRMODE_IMP,		2,		"PYA",	&MKCpu::OpCodeYA 		/*1a*/	}},
		{OPCODE_PAY_X,		{OPCODE_PAY_X,		ADDRMODE_IMP,		2,		"PYX",	&MKCpu::OpCodeYX 		/*1b*/	}},
		{OPCODE_PAY_Y,		{OPCODE_PAY_Y,		ADDRMODE_IMP,		2,		"PYY",	&MKCpu::OpCodeYY 		/*1c*/	}},
		{OPCODE_ORA_ABX,	{OPCODE_ORA_ABX,	ADDRMODE_ABX,		4,		"ORA",	&MKCpu::OpCodeOraAbx 		/*1d*/	}},
		{OPCODE_ASL_ABX,	{OPCODE_ASL_ABX,	ADDRMODE_ABX,		7,		"ASL",	&MKCpu::OpCodeAslAbx 		/*1e*/	}},
		{OPCODE_CLQ,		{OPCODE_CLQ,		ADDRMODE_IMP,		2,		"CLQ",	&MKCpu::OpCodeClq 		/*1f*/	}},
		{OPCODE_JSR_ABS,	{OPCODE_JSR_ABS,	ADDRMODE_ABS,		6,		"JSR",	&MKCpu::OpCodeJsrAbs 		/*20*/	}},
		{OPCODE_AND_IZX,	{OPCODE_AND_IZX,	ADDRMODE_IZX,		6,		"AND",	&MKCpu::OpCodeAndIzx 		/*21*/	}},
		{OPCODE_ILL_22,		{OPCODE_ILL_22,		ADDRMODE_UND,		0,		"ILL",	&MKCpu::OpCodeDud 		/*22*/	}},
		{OPCODE_ILL_23,		{OPCODE_ILL_23,		ADDRMODE_IZX,		8,		"RLA",	&MKCpu::OpCodeDud 		/*23*/	}},
		{OPCODE_BIT_ZP,		{OPCODE_BIT_ZP,		ADDRMODE_ZP,		3,		"BIT",	&MKCpu::OpCodeBitZp 		/*24*/	}},
		{OPCODE_AND_ZP,		{OPCODE_AND_ZP,		ADDRMODE_ZP,		3,		"AND",	&MKCpu::OpCodeAndZp 		/*25*/	}},
		{OPCODE_ROL_ZP,		{OPCODE_ROL_ZP,		ADDRMODE_ZP,		5,		"ROL",	&MKCpu::OpCodeRolZp 		/*26*/	}},
		{OPCODE_SEV,		{OPCODE_SEV,		ADDRMODE_IMP,		2,		"SEV",	&MKCpu::OpCodeSev 		/*27*/	}},
		{OPCODE_PLP,		{OPCODE_PLP,		ADDRMODE_IMP,		4,		"PLP",	&MKCpu::OpCodePlp		/*28*/	}},
		{OPCODE_AND_IMM,	{OPCODE_AND_IMM,	ADDRMODE_IMM,		2,		"AND",	&MKCpu::OpCodeAndImm 		/*29*/	}},
		{OPCODE_ROL,		{OPCODE_ROL,		ADDRMODE_ACC,		2,		"ROL",	&MKCpu::OpCodeRolAcc		/*2a*/	}},
		{OPCODE_SEZ,		{OPCODE_SEZ,		ADDRMODE_IMP,		2,		"SEZ",	&MKCpu::OpCodeSez 		/*2b*/	}},
		{OPCODE_BIT_ABS,	{OPCODE_BIT_ABS,	ADDRMODE_ABS,		4,		"BIT",	&MKCpu::OpCodeBitAbs 		/*2c*/	}},
		{OPCODE_AND_ABS,	{OPCODE_AND_ABS,	ADDRMODE_ABS,		4,		"AND",	&MKCpu::OpCodeAndAbs 		/*2d*/	}},
		{OPCODE_ROL_ABS,	{OPCODE_ROL_ABS,	ADDRMODE_ABS,		6,		"ROL",	&MKCpu::OpCodeRolAbs 		/*2e*/	}},
		{OPCODE_CLN,		{OPCODE_CLN,		ADDRMODE_IMP,		2,		"CLN",	&MKCpu::OpCodeCln 		/*2f*/	}},
		{OPCODE_BMI_REL,	{OPCODE_BMI_REL,	ADDRMODE_REL,		2,		"BMI",	&MKCpu::OpCodeBmiRel 		/*30*/	}},
		{OPCODE_AND_IZY,	{OPCODE_AND_IZY,	ADDRMODE_IZY,		5,		"AND",	&MKCpu::OpCodeAndIzy 		/*31*/	}},
		{OPCODE_PAZ_A,		{OPCODE_PAZ_A,		ADDRMODE_IMP,		3,		"PZA",	&MKCpu::OpCodeZA 		/*32*/	}},
		{OPCODE_PAZ_X,		{OPCODE_PAZ_X,		ADDRMODE_IMP,		3,		"PZX",	&MKCpu::OpCodeZX 		/*33*/	}},
		{OPCODE_PAZ_Y,		{OPCODE_PAZ_Y,		ADDRMODE_IMP,		3,		"PZY",	&MKCpu::OpCodeZY 		/*34*/	}},
		{OPCODE_AND_ZPX,	{OPCODE_AND_ZPX,	ADDRMODE_ZPX,		4,		"AND",	&MKCpu::OpCodeAndZpx 		/*35*/	}},
		{OPCODE_ROL_ZPX,	{OPCODE_ROL_ZPX,	ADDRMODE_ZPX,		6,		"ROL",	&MKCpu::OpCodeRolZpx 		/*36*/	}},
		{OPCODE_ILL_37,		{OPCODE_ILL_37,		ADDRMODE_ZPX,		6,		"RLA",	&MKCpu::OpCodeDud  		/*37*/	}},
		{OPCODE_SEC,		{OPCODE_SEC,		ADDRMODE_IMP,		2,		"SEC",	&MKCpu::OpCodeSec		/*38*/	}},
		{OPCODE_AND_ABY,	{OPCODE_AND_ABY,	ADDRMODE_ABY,		4,		"AND",	&MKCpu::OpCodeAndAby 		/*39*/	}},
		{OPCODE_ROT_A,		{OPCODE_ROT_A,		ADDRMODE_IMP,		3,		"RTA",	&MKCpu::OpCodeRTA 		/*3a*/	}},
		{OPCODE_ROT_X,		{OPCODE_ROT_X,		ADDRMODE_IMP,		3,		"RTX",	&MKCpu::OpCodeRTX 		/*3b*/	}},
		{OPCODE_ROT_Y,		{OPCODE_ROT_Y,		ADDRMODE_IMP,		3,		"RTY",	&MKCpu::OpCodeRTY 		/*3c*/	}},
		{OPCODE_AND_ABX,	{OPCODE_AND_ABX,	ADDRMODE_ABX,		4,		"AND",	&MKCpu::OpCodeAndAbx 		/*3d*/	}},
		{OPCODE_ROL_ABX,	{OPCODE_ROL_ABX,	ADDRMODE_ABX,		7,		"ROL",	&MKCpu::OpCodeRolAbx 		/*3e*/	}},
		{OPCODE_SEQ,		{OPCODE_SEQ,		ADDRMODE_IMP,		2,		"SEQ",	&MKCpu::OpCodeSeq		/*3f*/	}},
		{OPCODE_RTI,		{OPCODE_RTI,		ADDRMODE_IMP,		6,		"RTI",	&MKCpu::OpCodeRti		/*40*/	}},
		{OPCODE_EOR_IZX,	{OPCODE_EOR_IZX,	ADDRMODE_IZX,		6,		"EOR",	&MKCpu::OpCodeEorIzx 		/*41*/	}},
		{OPCODE_ROTX_A,		{OPCODE_ROTX_A,		ADDRMODE_IMP,		3,		"RXA",	&MKCpu::OpCodeRXA 		/*42*/	}},
		{OPCODE_ROTX_X,		{OPCODE_ROTX_X,		ADDRMODE_IMP,		3,		"RXX",	&MKCpu::OpCodeRXX 		/*43*/	}},
		{OPCODE_ROTX_Y,		{OPCODE_ROTX_Y,		ADDRMODE_IMP,		3,		"RXY",	&MKCpu::OpCodeRXY 		/*44*/	}},
		{OPCODE_EOR_ZP,		{OPCODE_EOR_ZP,		ADDRMODE_ZP,		3,		"EOR",	&MKCpu::OpCodeEorZp 		/*45*/	}},
		{OPCODE_LSR_ZP,		{OPCODE_LSR_ZP,		ADDRMODE_ZP,		5,		"LSR",	&MKCpu::OpCodeLsrZp 		/*46*/	}},
		{OPCODE_CLZ,		{OPCODE_CLZ,		ADDRMODE_IMP,		2,		"CLZ",	&MKCpu::OpCodeClz 		/*47*/	}},
		{OPCODE_PHA,		{OPCODE_PHA,		ADDRMODE_IMP,		3,		"PHA",	&MKCpu::OpCodePha 		/*48*/	}},
		{OPCODE_EOR_IMM,	{OPCODE_EOR_IMM,	ADDRMODE_IMM,		2,		"EOR",	&MKCpu::OpCodeEorImm 		/*49*/	}},
		{OPCODE_LSR,		{OPCODE_LSR,		ADDRMODE_ACC,		2,		"LSR",	&MKCpu::OpCodeLsrAcc		/*4a*/	}},
		{OPCODE_ILL_4B,		{OPCODE_ILL_4B,		ADDRMODE_IMM,		2,		"ALR",	&MKCpu::OpCodeDud 		/*4b*/	}},
		{OPCODE_JMP_ABS,	{OPCODE_JMP_ABS,	ADDRMODE_ABS,		3,		"JMP",	&MKCpu::OpCodeJmpAbs 		/*4c*/	}},
		{OPCODE_EOR_ABS,	{OPCODE_EOR_ABS,	ADDRMODE_ABS,		4,		"EOR",	&MKCpu::OpCodeEorAbs 		/*4d*/	}},
		{OPCODE_LSR_ABS,	{OPCODE_LSR_ABS,	ADDRMODE_ABS,		6,		"LSR",	&MKCpu::OpCodeLsrAbs 		/*4e*/	}},
		{OPCODE_ILL_4F,		{OPCODE_ILL_4F,		ADDRMODE_ABS,		6,		"SRE",	&MKCpu::OpCodeDud 		/*4f*/	}},
		{OPCODE_BVC_REL,	{OPCODE_BVC_REL,	ADDRMODE_REL,		2,		"BVC",	&MKCpu::OpCodeBvcRel 		/*50*/	}},
		{OPCODE_EOR_IZY,	{OPCODE_EOR_IZY,	ADDRMODE_IZY,		5,		"EOR",	&MKCpu::OpCodeEorIzy 		/*51*/	}},
		{OPCODE_ROTY_A,		{OPCODE_ROTY_A,		ADDRMODE_IMP,		3,		"RYA",	&MKCpu::OpCodeRYA 		/*52*/	}},
		{OPCODE_ROTY_X,		{OPCODE_ROTY_X,		ADDRMODE_IMP,		3,		"RYX",	&MKCpu::OpCodeRYX 		/*53*/	}},
		{OPCODE_ROTY_Y,		{OPCODE_ROTY_Y,		ADDRMODE_IMP,		3,		"RYY",	&MKCpu::OpCodeRYY 		/*54*/	}},
		{OPCODE_EOR_ZPX,	{OPCODE_EOR_ZPX,	ADDRMODE_ZPX,		4,		"EOR",	&MKCpu::OpCodeEorZpx 		/*55*/	}},
		{OPCODE_LSR_ZPX,	{OPCODE_LSR_ZPX,	ADDRMODE_ZPX,		6,		"LSR",	&MKCpu::OpCodeLsrZpx 		/*56*/	}},
		{OPCODE_ILL_57,		{OPCODE_ILL_57,		ADDRMODE_ZPX,		6,		"SRE",	&MKCpu::OpCodeDud 		/*57*/	}},
		{OPCODE_CLI,		{OPCODE_CLI,		ADDRMODE_IMP,		2,		"CLI",	&MKCpu::OpCodeCli		/*58*/	}},
		{OPCODE_EOR_ABY,	{OPCODE_EOR_ABY,	ADDRMODE_ABY,		4,		"EOR",	&MKCpu::OpCodeEorAby 		/*59*/	}},
		{OPCODE_ROTZ_A,		{OPCODE_ROTZ_A,		ADDRMODE_IMP,		3,		"RZA",	&MKCpu::OpCodeRZA 		/*5a*/	}},
		{OPCODE_ROTZ_X,		{OPCODE_ROTZ_X,		ADDRMODE_IMP,		3,		"RZX",	&MKCpu::OpCodeRZX 		/*5b*/	}},
		{OPCODE_ROTZ_Y,		{OPCODE_ROTZ_Y,		ADDRMODE_IMP,		3,		"RZY",	&MKCpu::OpCodeRZY 		/*5c*/	}},
		{OPCODE_EOR_ABX,	{OPCODE_EOR_ABX,	ADDRMODE_ABX,		4,		"EOR",	&MKCpu::OpCodeEorAbx 		/*5d*/	}},
		{OPCODE_LSR_ABX,	{OPCODE_LSR_ABX,	ADDRMODE_ABX,		7,		"LSR",	&MKCpu::OpCodeLsrAbx 		/*5e*/	}},
		{OPCODE_ILL_5F,		{OPCODE_ILL_5F,		ADDRMODE_ABX,		7,		"SRE",	&MKCpu::OpCodeDud 		/*5f*/	}},
		{OPCODE_RTS,		{OPCODE_RTS,		ADDRMODE_IMP,		6,		"RTS",	&MKCpu::OpCodeRts		/*60*/	}},
		{OPCODE_ADC_IZX,	{OPCODE_ADC_IZX,	ADDRMODE_IZX,		6,		"ADC",	&MKCpu::OpCodeAdcIzx 		/*61*/	}},
		{OPCODE_QFT_A,		{OPCODE_QFT_A,		ADDRMODE_IMP,		4,		"FTA",	&MKCpu::OpCodeFTA 		/*62*/	}},
		{OPCODE_QFT_X,		{OPCODE_QFT_X,		ADDRMODE_IMP,		4,		"FTX",	&MKCpu::OpCodeFTX 		/*63*/	}},
		{OPCODE_QFT_Y,		{OPCODE_QFT_Y,		ADDRMODE_IMP,		4,		"FTY",	&MKCpu::OpCodeFTY 		/*64*/	}},
		{OPCODE_ADC_ZP,		{OPCODE_ADC_ZP,		ADDRMODE_ZP,		3,		"ADC",	&MKCpu::OpCodeAdcZp 		/*65*/	}},
		{OPCODE_ROR_ZP,		{OPCODE_ROR_ZP,		ADDRMODE_ZP,		5,		"ROR",	&MKCpu::OpCodeRorZp 		/*66*/	}},
		{OPCODE_ILL_67,		{OPCODE_ILL_67,		ADDRMODE_ZP,		5,		"RRA",	&MKCpu::OpCodeDud 		/*67*/	}},
		{OPCODE_PLA,		{OPCODE_PLA,		ADDRMODE_IMP,		4,		"PLA",	&MKCpu::OpCodePla		/*68*/	}},
		{OPCODE_ADC_IMM,	{OPCODE_ADC_IMM,	ADDRMODE_IMM,		2,		"ADC",	&MKCpu::OpCodeAdcImm 		/*69*/	}},
		{OPCODE_ROR,		{OPCODE_ROR,		ADDRMODE_ACC,		2,		"ROR",	&MKCpu::OpCodeRorAcc		/*6a*/	}},
		{OPCODE_ILL_6B,		{OPCODE_ILL_6B,		ADDRMODE_IMM,		2,		"ARR",	&MKCpu::OpCodeDud 		/*6b*/	}},
		{OPCODE_JMP_IND,	{OPCODE_JMP_IND,	ADDRMODE_IND,		5,		"JMP",	&MKCpu::OpCodeJmpInd 		/*6c*/	}},
		{OPCODE_ADC_ABS,	{OPCODE_ADC_ABS,	ADDRMODE_ABS,		4,		"ADC",	&MKCpu::OpCodeAdcAbs 		/*6d*/	}},
		{OPCODE_ROR_ABS,	{OPCODE_ROR_ABS,	ADDRMODE_ABS,		6,		"ROR",	&MKCpu::OpCodeRorAbs 		/*6e*/	}},
		{OPCODE_ILL_6F,		{OPCODE_ILL_6F,		ADDRMODE_ABS,		6,		"RRA",	&MKCpu::OpCodeDud 		/*6f*/	}},
		{OPCODE_BVS_REL,	{OPCODE_BVS_REL,	ADDRMODE_REL,		2,		"BVS",	&MKCpu::OpCodeBvsRel 		/*70*/	}},
		{OPCODE_ADC_IZY,	{OPCODE_ADC_IZY,	ADDRMODE_IZY,		5,		"ADC",	&MKCpu::OpCodeAdcIzy 		/*71*/	}},
		{OPCODE_ILL_72,		{OPCODE_ILL_72,		ADDRMODE_UND,		0,		"ILL",	&MKCpu::OpCodeDud 		/*72*/	}},
		{OPCODE_ILL_73,		{OPCODE_ILL_73,		ADDRMODE_IZY,		8,		"RRA",	&MKCpu::OpCodeDud 		/*73*/	}},
		{OPCODE_ILL_74,		{OPCODE_ILL_74,		ADDRMODE_ZPX,		4,		"NOP",	&MKCpu::OpCodeDud 		/*74*/	}},
		{OPCODE_ADC_ZPX,	{OPCODE_ADC_ZPX,	ADDRMODE_ZPX,		4,		"ADC",	&MKCpu::OpCodeAdcZpx 		/*75*/	}},
		{OPCODE_ROR_ZPX,	{OPCODE_ROR_ZPX,	ADDRMODE_ZPX,		6,		"ROR",	&MKCpu::OpCodeRorZpx 		/*76*/	}},
		{OPCODE_ILL_77,		{OPCODE_ILL_77,		ADDRMODE_ZPX,		6,		"RRA",	&MKCpu::OpCodeDud 		/*77*/	}},
		{OPCODE_SEI,		{OPCODE_SEI,		ADDRMODE_IMP,		2,		"SEI",	&MKCpu::OpCodeSei		/*78*/	}},
		{OPCODE_ADC_ABY,	{OPCODE_ADC_ABY,	ADDRMODE_ABY,		4,		"ADC",	&MKCpu::OpCodeAdcAby 		/*79*/	}},
		{OPCODE_ILL_7A,		{OPCODE_ILL_7A,		ADDRMODE_IMP,		2,		"NOP",	&MKCpu::OpCodeDud 		/*7a*/	}},
		{OPCODE_ILL_7B,		{OPCODE_ILL_7B,		ADDRMODE_ABY,		7,		"RRA",	&MKCpu::OpCodeDud 		/*7b*/	}},
		{OPCODE_ILL_7C,		{OPCODE_ILL_7C,		ADDRMODE_ABX,		4,		"NOP",	&MKCpu::OpCodeDud 		/*7c*/	}},
		{OPCODE_ADC_ABX,	{OPCODE_ADC_ABX,	ADDRMODE_ABX,		4,		"ADC",	&MKCpu::OpCodeAdcAbx 		/*7d*/	}},
		{OPCODE_ROR_ABX,	{OPCODE_ROR_ABX,	ADDRMODE_ABX,		7,		"ROR",	&MKCpu::OpCodeRorAbx 		/*7e*/	}},
		{OPCODE_ILL_7F,		{OPCODE_ILL_7F,		ADDRMODE_ABX,		7,		"RRA",	&MKCpu::OpCodeDud 		/*7f*/	}},
		{OPCODE_ILL_80,		{OPCODE_ILL_80,		ADDRMODE_IMM,		2,		"NOP",	&MKCpu::OpCodeDud 		/*80*/	}},
		{OPCODE_STA_IZX,	{OPCODE_STA_IZX,	ADDRMODE_IZX,		6,		"STA",	&MKCpu::OpCodeStaIzx 		/*81*/	}},
		{OPCODE_ILL_82,		{OPCODE_ILL_82,		ADDRMODE_IMM,		2,		"NOP",	&MKCpu::OpCodeDud 		/*82*/	}},
		{OPCODE_ILL_83,		{OPCODE_ILL_83,		ADDRMODE_IZX,		6,		"SAX",	&MKCpu::OpCodeDud 		/*83*/	}},
		{OPCODE_STY_ZP,		{OPCODE_STY_ZP,		ADDRMODE_ZP,		3,		"STY",	&MKCpu::OpCodeStyZp 		/*84*/	}},
		{OPCODE_STA_ZP,		{OPCODE_STA_ZP,		ADDRMODE_ZP,		3,		"STA",	&MKCpu::OpCodeStaZp 		/*85*/	}},
		{OPCODE_STX_ZP,		{OPCODE_STX_ZP,		ADDRMODE_ZP,		3,		"STX",	&MKCpu::OpCodeStxZp 		/*86*/	}},
		{OPCODE_ILL_87,		{OPCODE_ILL_87,		ADDRMODE_ZP,		3,		"SAX",	&MKCpu::OpCodeDud 		/*87*/	}},
		{OPCODE_DEY,		{OPCODE_DEY,		ADDRMODE_IMP,		2,		"DEY",	&MKCpu::OpCodeDey 		/*88*/	}},
		{OPCODE_ILL_89,		{OPCODE_ILL_89,		ADDRMODE_IMM,		2,		"NOP",	&MKCpu::OpCodeDud 		/*89*/	}},
		{OPCODE_TXA,		{OPCODE_TXA,		ADDRMODE_IMP,		2,		"TXA",	&MKCpu::OpCodeTxa 		/*8a*/	}},
		{OPCODE_ILL_8B,		{OPCODE_ILL_8B,		ADDRMODE_IMM,		2,		"XAA",	&MKCpu::OpCodeDud 		/*8b*/	}},
		{OPCODE_STY_ABS,	{OPCODE_STY_ABS,	ADDRMODE_ABS,		4,		"STY",	&MKCpu::OpCodeStyAbs 		/*8c*/	}},
		{OPCODE_STA_ABS,	{OPCODE_STA_ABS,	ADDRMODE_ABS,		4,		"STA",	&MKCpu::OpCodeStaAbs 		/*8d*/	}},
		{OPCODE_STX_ABS,	{OPCODE_STX_ABS,	ADDRMODE_ABS,		4,		"STX",	&MKCpu::OpCodeStxAbs 		/*8e*/	}},
		{OPCODE_ILL_8F,		{OPCODE_ILL_8F,		ADDRMODE_ABS,		4,		"SAX",	&MKCpu::OpCodeDud 		/*8f*/	}},
		{OPCODE_BCC_REL,	{OPCODE_BCC_REL,	ADDRMODE_REL,		2,		"BCC",	&MKCpu::OpCodeBccRel 		/*90*/	}},
		{OPCODE_STA_IZY,	{OPCODE_STA_IZY,	ADDRMODE_IZY,		6,		"STA",	&MKCpu::OpCodeStaIzy 		/*91*/	}},
		{OPCODE_ILL_92,		{OPCODE_ILL_92,		ADDRMODE_UND,		0,		"ILL",	&MKCpu::OpCodeDud 		/*92*/	}},
		{OPCODE_ILL_93,		{OPCODE_ILL_93,		ADDRMODE_IZY,		6,		"AHX",	&MKCpu::OpCodeDud 		/*93*/	}},
		{OPCODE_STY_ZPX,	{OPCODE_STY_ZPX,	ADDRMODE_ZPX,		4,		"STY",	&MKCpu::OpCodeStyZpx 		/*94*/	}},
		{OPCODE_STA_ZPX,	{OPCODE_STA_ZPX,	ADDRMODE_ZPX,		4,		"STA",	&MKCpu::OpCodeStaZpx 		/*95*/	}},
		{OPCODE_STX_ZPY,	{OPCODE_STX_ZPY,	ADDRMODE_ZPY,		4,		"STX",	&MKCpu::OpCodeStxZpy 		/*96*/	}},
		{OPCODE_ILL_97,		{OPCODE_ILL_97,		ADDRMODE_ZPY,		4,		"SAX",	&MKCpu::OpCodeDud 		/*97*/	}},
		{OPCODE_TYA,		{OPCODE_TYA,		ADDRMODE_IMP,		2,		"TYA",	&MKCpu::OpCodeTya 		/*98*/	}},
		{OPCODE_STA_ABY,	{OPCODE_STA_ABY,	ADDRMODE_ABY,		5,		"STA",	&MKCpu::OpCodeStaAby 		/*99*/	}},
		{OPCODE_TXS,		{OPCODE_TXS,		ADDRMODE_IMP,		2,		"TXS",	&MKCpu::OpCodeTxs 		/*9a*/	}},
		{OPCODE_ILL_9B,		{OPCODE_ILL_9B,		ADDRMODE_ABY,		5,		"TAS",	&MKCpu::OpCodeDud 		/*9b*/	}},
		{OPCODE_ILL_9C,		{OPCODE_ILL_9C,		ADDRMODE_ABX,		5,		"SHY",	&MKCpu::OpCodeDud 		/*9c*/	}},
		{OPCODE_STA_ABX,	{OPCODE_STA_ABX,	ADDRMODE_ABX,		5,		"STA",	&MKCpu::OpCodeStaAbx 		/*9d*/	}},
		{OPCODE_ILL_9E,		{OPCODE_ILL_9E,		ADDRMODE_ABY,		5,		"SHX",	&MKCpu::OpCodeDud 		/*9e*/	}},
		{OPCODE_ILL_9F,		{OPCODE_ILL_9F,		ADDRMODE_ABY,		5,		"AHX",	&MKCpu::OpCodeDud 		/*9f*/	}},
		{OPCODE_LDY_IMM,	{OPCODE_LDY_IMM,	ADDRMODE_IMM,		2,		"LDY",	&MKCpu::OpCodeLdyImm 		/*a0*/	}},
		{OPCODE_LDA_IZX,	{OPCODE_LDA_IZX,	ADDRMODE_IZX,		6,		"LDA",	&MKCpu::OpCodeLdaIzx 		/*a1*/	}},
		{OPCODE_LDX_IMM,	{OPCODE_LDX_IMM,	ADDRMODE_IMM,		2,		"LDX",	&MKCpu::OpCodeLdxImm 		/*a2*/	}},
		{OPCODE_ILL_A3,		{OPCODE_ILL_A3,		ADDRMODE_IZX,		6,		"LAX",	&MKCpu::OpCodeDud 		/*a3*/	}},
		{OPCODE_LDY_ZP,		{OPCODE_LDY_ZP,		ADDRMODE_ZP,		3,		"LDY",	&MKCpu::OpCodeLdyZp 		/*a4*/	}},
		{OPCODE_LDA_ZP,		{OPCODE_LDA_ZP,		ADDRMODE_ZP,		3,		"LDA",	&MKCpu::OpCodeLdaZp 		/*a5*/	}},
		{OPCODE_LDX_ZP,		{OPCODE_LDX_ZP,		ADDRMODE_ZP,		3,		"LDX",	&MKCpu::OpCodeLdxZp 		/*a6*/	}},
		{OPCODE_ILL_A7,		{OPCODE_ILL_A7,		ADDRMODE_ZP,		3,		"LAX",	&MKCpu::OpCodeDud 		/*a7*/	}},
		{OPCODE_TAY,		{OPCODE_TAY,		ADDRMODE_IMP,		2,		"TAY",	&MKCpu::OpCodeTay 		/*a8*/	}},
		{OPCODE_LDA_IMM,	{OPCODE_LDA_IMM,	ADDRMODE_IMM,		2,		"LDA",	&MKCpu::OpCodeLdaImm 		/*a9*/	}},
		{OPCODE_TAX,		{OPCODE_TAX,		ADDRMODE_IMP,		2,		"TAX",	&MKCpu::OpCodeTax 		/*aa*/	}},
		{OPCODE_ILL_AB,		{OPCODE_ILL_AB,		ADDRMODE_IMM,		2,		"LAX",	&MKCpu::OpCodeDud 		/*ab*/	}},
		{OPCODE_LDY_ABS,	{OPCODE_LDY_ABS,	ADDRMODE_ABS,		4,		"LDY",	&MKCpu::OpCodeLdyAbs 		/*ac*/	}},
		{OPCODE_LDA_ABS,	{OPCODE_LDA_ABS,	ADDRMODE_ABS,		4,		"LDA",	&MKCpu::OpCodeLdaAbs 		/*ad*/	}},
		{OPCODE_LDX_ABS,	{OPCODE_LDX_ABS,	ADDRMODE_ABS,		4,		"LDX",	&MKCpu::OpCodeLdxAbs 		/*ae*/	}},
		{OPCODE_ILL_AF,		{OPCODE_ILL_AF,		ADDRMODE_ABS,		4,		"LAX",	&MKCpu::OpCodeDud 		/*af*/	}},
		{OPCODE_BCS_REL,	{OPCODE_BCS_REL,	ADDRMODE_REL,		2,		"BCS",	&MKCpu::OpCodeBcsRel 		/*b0*/	}},
		{OPCODE_LDA_IZY,	{OPCODE_LDA_IZY,	ADDRMODE_IZY,		5,		"LDA",	&MKCpu::OpCodeLdaIzy 		/*b1*/	}},
		{OPCODE_ILL_B2,		{OPCODE_ILL_B2,		ADDRMODE_UND,		0,		"ILL",	&MKCpu::OpCodeDud 		/*b2*/	}},
		{OPCODE_ILL_B3,		{OPCODE_ILL_B3,		ADDRMODE_IZY,		5,		"LAX",	&MKCpu::OpCodeDud 		/*b3*/	}},
		{OPCODE_LDY_ZPX,	{OPCODE_LDY_ZPX,	ADDRMODE_ZPX,		4,		"LDY",	&MKCpu::OpCodeLdyZpx 		/*b4*/	}},
		{OPCODE_LDA_ZPX,	{OPCODE_LDA_ZPX,	ADDRMODE_ZPX,		4,		"LDA",	&MKCpu::OpCodeLdaZpx 		/*b5*/	}},
		{OPCODE_LDX_ZPY,	{OPCODE_LDX_ZPY,	ADDRMODE_ZPY,		4,		"LDX",	&MKCpu::OpCodeLdxZpy 		/*b6*/	}},
		{OPCODE_ILL_B7,		{OPCODE_ILL_B7,		ADDRMODE_ZPY,		4,		"LAX",	&MKCpu::OpCodeDud 		/*b7*/	}},
		{OPCODE_CLV,		{OPCODE_CLV,		ADDRMODE_IMP,		2,		"CLV",	&MKCpu::OpCodeClv 		/*b8*/	}},
		{OPCODE_LDA_ABY,	{OPCODE_LDA_ABY,	ADDRMODE_ABY,		4,		"LDA",	&MKCpu::OpCodeLdaAby 		/*b9*/	}},
		{OPCODE_TSX,		{OPCODE_TSX,		ADDRMODE_IMP,		2,		"TSX",	&MKCpu::OpCodeTsx 		/*ba*/	}},
		{OPCODE_ILL_BB,		{OPCODE_ILL_BB,		ADDRMODE_ABY,		4,		"LAS",	&MKCpu::OpCodeDud 		/*bb*/	}},
		{OPCODE_LDY_ABX,	{OPCODE_LDY_ABX,	ADDRMODE_ABX,		4,		"LDY",	&MKCpu::OpCodeLdyAbx 		/*bc*/	}},
		{OPCODE_LDA_ABX,	{OPCODE_LDA_ABX,	ADDRMODE_ABX,		4,		"LDA",	&MKCpu::OpCodeLdaAbx 		/*bd*/	}},
		{OPCODE_LDX_ABY,	{OPCODE_LDX_ABY,	ADDRMODE_ABY,		4,		"LDX",	&MKCpu::OpCodeLdxAby 		/*be*/	}},
		{OPCODE_ILL_BF,		{OPCODE_ILL_BF,		ADDRMODE_ABY,		2,		"NOP",	&MKCpu::OpCodeNop 		/*bf*/	}},
		{OPCODE_CPY_IMM,	{OPCODE_CPY_IMM,	ADDRMODE_IMM,		2,		"CPY",	&MKCpu::OpCodeCpyImm 		/*c0*/	}},
		{OPCODE_CMP_IZX,	{OPCODE_CMP_IZX,	ADDRMODE_IZX,		6,		"CMP",	&MKCpu::OpCodeCmpIzx 		/*c1*/	}},
		{OPCODE_ILL_C2,		{OPCODE_ILL_C2,		ADDRMODE_IMM,		2,		"NOP",	&MKCpu::OpCodeDud 		/*c2*/	}},
		{OPCODE_ILL_C3,		{OPCODE_ILL_C3,		ADDRMODE_IZX,		8,		"DCP",	&MKCpu::OpCodeDud,		/*c3*/	}},
		{OPCODE_CPY_ZP,		{OPCODE_CPY_ZP,		ADDRMODE_ZP,		3,		"CPY",	&MKCpu::OpCodeCpyZp 		/*c4*/	}},
		{OPCODE_CMP_ZP,		{OPCODE_CMP_ZP,		ADDRMODE_ZP,		3,		"CMP",	&MKCpu::OpCodeCmpZp 		/*c5*/	}},
		{OPCODE_DEC_ZP,		{OPCODE_DEC_ZP,		ADDRMODE_ZP,		5,		"DEC",	&MKCpu::OpCodeDecZp 		/*c6*/	}},
		{OPCODE_ILL_C7,		{OPCODE_ILL_C7,		ADDRMODE_ZP,		5,		"DCP",	&MKCpu::OpCodeDud 		/*c7*/	}},
		{OPCODE_INY,		{OPCODE_INY,		ADDRMODE_IMP,		2,		"INY",	&MKCpu::OpCodeIny 		/*c8*/	}},
		{OPCODE_CMP_IMM,	{OPCODE_CMP_IMM,	ADDRMODE_IMM,		2,		"CMP",	&MKCpu::OpCodeCmpImm 		/*c9*/	}},
		{OPCODE_DEX,		{OPCODE_DEX,		ADDRMODE_IMP,		2,		"DEX",	&MKCpu::OpCodeDex 		/*ca*/	}},
		{OPCODE_ILL_CB,		{OPCODE_ILL_CB,		ADDRMODE_IMM,		2,		"AXS",	&MKCpu::OpCodeDud 		/*cb*/	}},
		{OPCODE_CPY_ABS,	{OPCODE_CPY_ABS,	ADDRMODE_ABS,		4,		"CPY",	&MKCpu::OpCodeCpyAbs	 	/*cc*/	}},
		{OPCODE_CMP_ABS,	{OPCODE_CMP_ABS,	ADDRMODE_ABS,		4,		"CMP",	&MKCpu::OpCodeCmpAbs 		/*cd*/	}},
		{OPCODE_DEC_ABS,	{OPCODE_DEC_ABS,	ADDRMODE_ABS,		6,		"DEC",	&MKCpu::OpCodeDecAbs 		/*ce*/	}},
		{OPCODE_ILL_CF,		{OPCODE_ILL_CF,		ADDRMODE_ABS,		6,		"DCP",	&MKCpu::OpCodeDud 		/*cf*/	}},
		{OPCODE_BNE_REL,	{OPCODE_BNE_REL,	ADDRMODE_REL,		2,		"BNE",	&MKCpu::OpCodeBneRel 		/*d0*/	}},
		{OPCODE_CMP_IZY,	{OPCODE_CMP_IZY,	ADDRMODE_IZY,		5,		"CMP",	&MKCpu::OpCodeCmpIzy 		/*d1*/	}},
		{OPCODE_ILL_D2,		{OPCODE_ILL_D2,		ADDRMODE_UND,		0,		"ILL",	&MKCpu::OpCodeDud 		/*d2*/	}},
		{OPCODE_ILL_D3,		{OPCODE_ILL_D3,		ADDRMODE_IZY,		8,		"DCP",	&MKCpu::OpCodeDud 		/*d3*/	}},
		{OPCODE_ILL_D4,		{OPCODE_ILL_D4,		ADDRMODE_ZPX,		4,		"NOP",	&MKCpu::OpCodeDud 		/*d4*/	}},
		{OPCODE_CMP_ZPX,	{OPCODE_CMP_ZPX,	ADDRMODE_ZPX,		4,		"CMP",	&MKCpu::OpCodeCmpZpx 		/*d5*/	}},
		{OPCODE_DEC_ZPX,	{OPCODE_DEC_ZPX,	ADDRMODE_ZPX,		6,		"DEC",	&MKCpu::OpCodeDecZpx 		/*d6*/	}},
		{OPCODE_ILL_D7,		{OPCODE_ILL_D7,		ADDRMODE_ZPX,		6,		"DCP",	&MKCpu::OpCodeDud 		/*d7*/	}},
		{OPCODE_CLD,		{OPCODE_CLD,		ADDRMODE_IMP,		2,		"CLD",	&MKCpu::OpCodeCld 		/*d8*/	}},
		{OPCODE_CMP_ABY,	{OPCODE_CMP_ABY,	ADDRMODE_ABY,		4,		"CMP",	&MKCpu::OpCodeCmpAby 		/*d9*/	}},
		{OPCODE_ILL_DA,		{OPCODE_ILL_DA,		ADDRMODE_IMP,		2,		"NOP",	&MKCpu::OpCodeDud 		/*da*/	}},
		{OPCODE_ILL_DB,		{OPCODE_ILL_DB,		ADDRMODE_ABY,		7,		"DCP",	&MKCpu::OpCodeDud 		/*db*/	}},
		{OPCODE_ILL_DC,		{OPCODE_ILL_DC,		ADDRMODE_ABX,		4,		"NOP",	&MKCpu::OpCodeDud 		/*dc*/	}},
		{OPCODE_CMP_ABX,	{OPCODE_CMP_ABX,	ADDRMODE_ABX,		4,		"CMP",	&MKCpu::OpCodeCmpAbx 		/*dd*/	}},
		{OPCODE_DEC_ABX,	{OPCODE_DEC_ABX,	ADDRMODE_ABX,		7,		"DEC",	&MKCpu::OpCodeDecAbx 		/*de*/	}},
		{OPCODE_ILL_DF,		{OPCODE_ILL_DF,		ADDRMODE_ABX,		7,		"DCP",	&MKCpu::OpCodeDud 		/*df*/	}},
		{OPCODE_CPX_IMM,	{OPCODE_CPX_IMM,	ADDRMODE_IMM,		2,		"CPX",	&MKCpu::OpCodeCpxImm 		/*e0*/	}},
		{OPCODE_SBC_IZX,	{OPCODE_SBC_IZX,	ADDRMODE_IZX,		6,		"SBC",	&MKCpu::OpCodeSbcIzx 		/*e1*/	}},
		{OPCODE_ILL_E2,		{OPCODE_ILL_E2,		ADDRMODE_IMM,		2,		"NOP",	&MKCpu::OpCodeDud 		/*e2*/	}},
		{OPCODE_ILL_E3,		{OPCODE_ILL_E3,		ADDRMODE_IZX,		8,		"ISC",	&MKCpu::OpCodeDud 		/*e3*/	}},
		{OPCODE_CPX_ZP,		{OPCODE_CPX_ZP,		ADDRMODE_ZP,		3,		"CPX",	&MKCpu::OpCodeCpxZp 		/*e4*/	}},
		{OPCODE_SBC_ZP,		{OPCODE_SBC_ZP,		ADDRMODE_ZP,		3,		"SBC",	&MKCpu::OpCodeSbcZp 		/*e5*/	}},
		{OPCODE_INC_ZP,		{OPCODE_INC_ZP,		ADDRMODE_ZP,		5,		"INC",	&MKCpu::OpCodeIncZp 		/*e6*/	}},
		{OPCODE_ILL_E7,		{OPCODE_ILL_E7,		ADDRMODE_ZP,		5,		"ISC",	&MKCpu::OpCodeDud 		/*e7*/	}},
		{OPCODE_INX,		{OPCODE_INX,		ADDRMODE_IMP,		2,		"INX",	&MKCpu::OpCodeInx 		/*e8*/	}},
		{OPCODE_SBC_IMM,	{OPCODE_SBC_IMM,	ADDRMODE_IMM,		2,		"SBC",	&MKCpu::OpCodeSbcImm 		/*e9*/	}},
		{OPCODE_NOP,		{OPCODE_NOP,		ADDRMODE_IMP,		2,		"NOP",	&MKCpu::OpCodeNop 		/*ea*/	}},
		{OPCODE_ILL_EB,		{OPCODE_ILL_EB,		ADDRMODE_IMM,		2,		"SBC",	&MKCpu::OpCodeDud 		/*eb*/	}},
		{OPCODE_CPX_ABS,	{OPCODE_CPX_ABS,	ADDRMODE_ABS,		4,		"CPX",	&MKCpu::OpCodeCpxAbs 		/*ec*/	}},
		{OPCODE_SBC_ABS,	{OPCODE_SBC_ABS,	ADDRMODE_ABS,		4,		"SBC",	&MKCpu::OpCodeSbcAbs 		/*ed*/	}},
		{OPCODE_INC_ABS,	{OPCODE_INC_ABS,	ADDRMODE_ABS,		6,		"INC",	&MKCpu::OpCodeIncAbs 		/*ee*/	}},
		{OPCODE_ILL_EF,		{OPCODE_ILL_EF,		ADDRMODE_ABS,		6,		"ISC",	&MKCpu::OpCodeDud 		/*ef*/	}},
		{OPCODE_BEQ_REL,	{OPCODE_BEQ_REL,	ADDRMODE_REL,		2,		"BEQ",	&MKCpu::OpCodeBeqRel 		/*f0*/	}},
		{OPCODE_SBC_IZY,	{OPCODE_SBC_IZY,	ADDRMODE_IZY,		5,		"SBC",	&MKCpu::OpCodeSbcIzy 		/*f1*/	}},
		{OPCODE_ILL_F2,		{OPCODE_ILL_F2,		ADDRMODE_UND,		0,		"ILL",	&MKCpu::OpCodeDud 		/*f2*/	}},
		{OPCODE_ILL_F3,		{OPCODE_ILL_F3,		ADDRMODE_IZY,		8,		"ISC",	&MKCpu::OpCodeDud 		/*f3*/	}},
		{OPCODE_ILL_F4,		{OPCODE_ILL_F4,		ADDRMODE_ZPX,		4,		"NOP",	&MKCpu::OpCodeDud 		/*f4*/	}},
		{OPCODE_SBC_ZPX,	{OPCODE_SBC_ZPX,	ADDRMODE_ZPX,		4,		"SBC",	&MKCpu::OpCodeSbcZpx 		/*f5*/	}},
		{OPCODE_INC_ZPX,	{OPCODE_INC_ZPX,	ADDRMODE_ZPX,		6,		"INC",	&MKCpu::OpCodeIncZpx 		/*f6*/	}},
		{OPCODE_QZN_Z,		{OPCODE_QZN_Z,		ADDRMODE_IMP,		6,		"QZZ",	&MKCpu::OpCodeQzZero 		/*f7*/	}},
		{OPCODE_SED,		{OPCODE_SED,		ADDRMODE_IMP,		2,		"SED",	&MKCpu::OpCodeSed 		/*f8*/	}},
		{OPCODE_SBC_ABY,	{OPCODE_SBC_ABY,	ADDRMODE_ABY,		4,		"SBC",	&MKCpu::OpCodeSbcAby 		/*f9*/	}},
		{OPCODE_QZN_S,		{OPCODE_QZN_S,		ADDRMODE_IMP,		2,		"QZS",	&MKCpu::OpCodeQzSign 		/*fa*/	}},
		{OPCODE_QZN_C,		{OPCODE_QZN_C,		ADDRMODE_IMP,		2,		"QZC",	&MKCpu::OpCodeQzCarry 		/*fb*/	}},
		{OPCODE_ILL_FC,		{OPCODE_ILL_FC,		ADDRMODE_IMP,		2,		"QZO",	&MKCpu::OpCodeQzOver 		/*fc*/	}},
		{OPCODE_SBC_ABX,	{OPCODE_SBC_ABX,	ADDRMODE_ABX,		4,		"SBC",	&MKCpu::OpCodeSbcAbx 		/*fd*/	}},
		{OPCODE_INC_ABX,	{OPCODE_INC_ABX,	ADDRMODE_ABX,		7,		"INC",	&MKCpu::OpCodeIncAbx 		/*fe*/	}},
	};
	mOpCodesMap = myOpCodesMap;
	mReg.isAccQ = false;
	mReg.isXQ = false;
	mReg.isYQ = false;
	mReg.isCarryQ = false;
	mReg.Acc = 0;
	mReg.Acc16 = 0;
	mReg.Flags = 0;
	mReg.IndX = 0;
	mReg.IndY = 0;
	mReg.Ptr16 = 0;
	mReg.PtrAddr = 0;
	mReg.PtrStack = 0xFF;	// top of stack	
	mReg.SoftIrq = false;
	mReg.IrqPending = false;
	mReg.CyclesLeft = 1;
	mReg.PageBoundary = false;
	mLocalMem = false;
	mExitAtLastRTS = true;
	mEnableHistory = false;	// performance decrease when enabled
	if (NULL == mpMem) {
		mpMem = new Memory();
		if (NULL == mpMem) {
			throw MKGenException("Unable to allocate memory!");
		}
		mLocalMem = true;
	}	
	// Set default BRK vector ($FFFE -> $FFF0)
	mpMem->Poke8bitImg(0xFFFE,0xF0); // LSB
	mpMem->Poke8bitImg(0xFFFF,0xFF); // MSB
	// Put RTI opcode at BRK procedure address.
	mpMem->Poke8bitImg(0xFFF0, OPCODE_RTI);
	// Set default RESET vector ($FFFC -> $0200)
	mpMem->Poke8bitImg(0xFFFC,0x00); // LSB
	mpMem->Poke8bitImg(0xFFFD,0x02); // MSB
	// Set BRK code at the RESET procedure address.
	mpMem->Poke8bitImg(0x0200,OPCODE_BRK);

	// Initialize the QCPU
	qReg = Qrack::CreateQuantumInterface(qUnitEngine, qUnitSubEngine1, 25, 0);
	if (NULL == qReg) {
		throw MKGenException("Unable to acquire QUnit");
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		CollapseAccQ()
 * Purpose:		Collapse Acc register state if it is in superposition
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::CollapseAccQ() {
	if (mReg.isAccQ) {
		mReg.Acc = qReg->MReg(REGS_ACC_Q, REG_LEN);
	}
	mReg.isAccQ = false;
}

/*
 *--------------------------------------------------------------------
 * Method:		CollapseXQ()
 * Purpose:		Collapse X register state if it is in superposition
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::CollapseXQ() {
	if (mReg.isXQ) {
		mReg.IndX = qReg->MReg(REGS_INDX_Q, REG_LEN);
	}
	mReg.isXQ = false;
}

/*
 *--------------------------------------------------------------------
 * Method:		CollapseYQ()
 * Purpose:		Collapse Y register state if it is in superposition
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::CollapseYQ() {
	if (mReg.isYQ) {
		mReg.IndY = qReg->MReg(REGS_INDY_Q, REG_LEN);
	}
	mReg.isYQ = false;
}

/*
 *--------------------------------------------------------------------
 * Method:		CollapseCarryQ()
 * Purpose:		Collapse carry bit if it is in superposition
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::CollapseCarryQ() {
	if (mReg.isCarryQ) {
		mReg.Flags &= ~FLAGS_CARRY;
		if (qReg->M(FLAGS_CARRY_Q)) {
			mReg.Flags |= FLAGS_CARRY;
		}
	}
	mReg.isCarryQ = false;
}

/*
 *--------------------------------------------------------------------
 * Method:		PrepareAccQ()
 * Purpose:		Prepare Acc register qubit state for quantum operations
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::PrepareAccQ() {
	if (!(mReg.isAccQ)) {
		qReg->SetReg(REGS_ACC_Q, REG_LEN, mReg.Acc);
	}
	mReg.isAccQ = true;
}

/*
 *--------------------------------------------------------------------
 * Method:		PrepareXQ()
 * Purpose:		Prepare X register qubit state for quantum operations
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::PrepareXQ() {
	if (!(mReg.isXQ)) {
		qReg->SetReg(REGS_INDX_Q, REG_LEN, mReg.IndX);
	}
	mReg.isXQ = true;
}

/*
 *--------------------------------------------------------------------
 * Method:		PrepareYQ()
 * Purpose:		Prepare Y register qubit state for quantum operations
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::PrepareYQ() {
	if (!(mReg.isYQ)) {
		qReg->SetReg(REGS_INDY_Q, REG_LEN, mReg.IndY);
	}
	mReg.isYQ = true;
}

/*
 *--------------------------------------------------------------------
 * Method:		PrepareCarryQ()
 * Purpose:		Prepare carry flag qubit state for quantum operations
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::PrepareCarryQ() {
	if (!(mReg.isCarryQ)) {
		qReg->SetBit(FLAGS_CARRY_Q, (mReg.Flags & FLAGS_CARRY) > 0);
	}
	mReg.isCarryQ = true;
}

/*
 *--------------------------------------------------------------------
 * Method:		RotateClassical()
 * Purpose:		Perform classical equivalent of quantum rotation
 * Arguments:		reg - input classical value of register
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
unsigned char MKCpu::RotateClassical(unsigned char reg) {
	//unsigned char offset = reg & 0x7F;
	//bool sign = reg & 0x80;
	//if (sign && offset >= 0x40) reg = 0x80 | ((~offset) & 0x7F); 
	//else if (sign && offset < 0x40) reg = offset;
	//else if ((!sign) && offset >= 0x40) reg |= 0x80;
	//else if ((!sign) && offset < 0x40) reg = ((~offset) & 0x7F);
	//return reg;
	return reg + 0x80;
}	

/*
 *--------------------------------------------------------------------
 * Method:		SetFlags()
 * Purpose:		Set CPU status flags ZERO and SIGN based on argument
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::SetFlags(unsigned char reg)
{
	SetFlag((0 == reg), FLAGS_ZERO);
	SetFlag(((reg & FLAGS_SIGN) == FLAGS_SIGN), FLAGS_SIGN);
}

/*
 *--------------------------------------------------------------------
 * Method:		SetFlagsReg()
 * Purpose:		Set CPU status flags ZERO and SIGN based on Acc, X or Y
 * Arguments:		regStart - qubit start index of register
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::SetFlagsReg(unsigned char regStart)
{
	if (mReg.Flags & FLAGS_QUANTUM) {
		if (mReg.Flags & FLAGS_ZERO) qReg->ZeroPhaseFlip(regStart, REG_LEN);
		if (mReg.Flags & FLAGS_SIGN) qReg->Z(regStart + REG_LEN - 1);
	}
	else {
		unsigned char toTest = 0;
		if (regStart == REGS_ACC_Q) {
			CollapseAccQ();
			toTest = mReg.Acc;
		}
		else if (regStart == REGS_INDX_Q) {
			CollapseXQ();
			toTest = mReg.IndX;
		}
		SetFlag((0 == toTest), FLAGS_ZERO);
		SetFlag(((toTest & FLAGS_SIGN) == FLAGS_SIGN), FLAGS_SIGN);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		MeasureFlagsQ()
 * Purpose:		Measure the qubit flags, and set the
			corresponding classical flags.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::MeasureFlagsQ()
{
	if (qReg->M(FLAGS_CARRY_Q)) {
		mReg.Flags |= FLAGS_CARRY;
	}
	else {
		mReg.Flags &= ~FLAGS_CARRY;
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		ShiftLeft()
 * Purpose:		Arithmetic shift left (1 bit), set Carry flag, shift 0
 *            into bit #0. Update flags NZ.
 * Arguments:	arg8 - 8-bit value
 * Returns:		8-bit value after shift
 *--------------------------------------------------------------------
 */
void MKCpu::ShiftLeftQ()
{
	if (mReg.isAccQ || mReg.isCarryQ) {
		PrepareCarryQ();
		PrepareAccQ();
		// set Carry flag based on original bit #7
		qReg->Swap(REGS_ACC_Q + REG_LEN, FLAGS_CARRY_Q);
		qReg->SetBit(REGS_ACC_Q + REG_LEN, 0);
		qReg->ROL(1, REGS_ACC_Q, REG_LEN + 1);
		qReg->Swap(REGS_ACC_Q + REG_LEN, FLAGS_CARRY_Q);
	}
}
unsigned char MKCpu::ShiftLeft(unsigned char arg8)
{
	// set Carry flag based on original bit #7
	SetFlag(((arg8 & FLAGS_SIGN) == FLAGS_SIGN), FLAGS_CARRY);
	arg8 = arg8 << 1;	// shift left
	arg8 &= 0xFE;			// shift 0 into bit #0
	
	SetFlags(arg8);
	
	return arg8;
} 

/*
 *--------------------------------------------------------------------
 * Method:		ShiftRight()
 * Purpose:		Logical Shift Right, update flags NZC.
 * Arguments:	arg8 - byte value
 * Returns:		unsigned char (byte) - after shift
 *--------------------------------------------------------------------
 */
void MKCpu::ShiftRightQ()
{
	if (mReg.isAccQ || mReg.isCarryQ) {
		PrepareCarryQ();
		PrepareAccQ();
		// set Carry flag based on original bit #7
		qReg->Swap(REGS_ACC_Q + REG_LEN, FLAGS_CARRY_Q);
		qReg->SetBit(REGS_ACC_Q + REG_LEN, false);
		qReg->ROR(1, REGS_ACC_Q, REG_LEN + 1);
		qReg->Swap(REGS_ACC_Q + REG_LEN, FLAGS_CARRY_Q);
	}
}
unsigned char MKCpu::ShiftRight(unsigned char arg8)
{
	SetFlag(((arg8 & 0x01) == 0x01), FLAGS_CARRY);
	arg8 = arg8 >> 1;
	arg8 &= 0x7F;	// unset bit #7
	SetFlags(arg8);
	
	return arg8;
}

/*
 *--------------------------------------------------------------------
 * Method:		RotateLeft()
 * Purpose:		Rotate left, Carry to bit 0, bit 7 to Carry, update
 							flags N and Z.
 * Arguments: arg8 - byte value to rotate
 * Returns:		unsigned char (byte) - rotated value
 *--------------------------------------------------------------------
 */
void MKCpu::RotateLeftQ()
{
	if (mReg.isAccQ || mReg.isCarryQ) {
		PrepareCarryQ();
		PrepareAccQ();
		// set Carry flag based on original bit #7
		qReg->Swap(REGS_ACC_Q + REG_LEN, FLAGS_CARRY_Q);
		qReg->ROL(1, REGS_ACC_Q, REG_LEN + 1);
		qReg->Swap(REGS_ACC_Q + REG_LEN, FLAGS_CARRY_Q);
	}
} 
unsigned char MKCpu::RotateLeft(unsigned char arg8)
{
	unsigned char tmp8 = 0;
	
	tmp8 = arg8;
	arg8 = arg8 << 1;
	// Carry goes to bit #0.
	if (mReg.Flags & FLAGS_CARRY) {
		arg8 |= 0x01;
	} else {
		arg8 &= 0xFE;
	}
	// Original (before ROL) bit #7 goes to Carry.
	SetFlag(((tmp8 & FLAGS_SIGN) == FLAGS_SIGN), FLAGS_CARRY);
	SetFlags(arg8);	// set flags Z and N	
	
	return arg8;
}

/*
 *--------------------------------------------------------------------
 * Method:		RotateRight()
 * Purpose:		Rotate Right, Carry to bit 7, bit 0 to Carry, update
 							flags N and Z.
 * Arguments: arg8 - byte value to rotate
 * Returns:		unsigned char (byte) - rotated value
 *--------------------------------------------------------------------
 */
void MKCpu::RotateRightQ()
{
	if (mReg.isAccQ || mReg.isCarryQ) {
		PrepareCarryQ();
		PrepareAccQ();
		// set Carry flag based on original bit #7
		qReg->Swap(REGS_ACC_Q + REG_LEN, FLAGS_CARRY_Q);
		qReg->ROR(1, REGS_ACC_Q, REG_LEN + 1);
		qReg->Swap(REGS_ACC_Q + REG_LEN, FLAGS_CARRY_Q);
	}
} 
unsigned char MKCpu::RotateRight(unsigned char arg8)
{
	unsigned char tmp8 = 0;
	
	tmp8 = arg8;
	arg8 = arg8 >> 1;
	// Carry goes to bit #7.
	if (CheckFlag(FLAGS_CARRY)) {
		arg8 |= 0x80;
	} else {
		arg8 &= 0x7F;
	}
	// Original (before ROR) bit #0 goes to Carry.
	SetFlag(((tmp8 & 0x01) == 0x01), FLAGS_CARRY);
	SetFlags(arg8);	// set flags Z and N	
	
	return arg8;
}

/*
 *--------------------------------------------------------------------
 * Method:		GetArg16()
 * Purpose:		Get 2-byte argument, add offset, increase PC.
 * Arguments:	addr - address of argument in memory
 *						offs - offset to be added to returned value
 * Returns:		16-bit address
 *--------------------------------------------------------------------
 */
unsigned short MKCpu::GetArg16(unsigned char offs)
{
	unsigned short ret = 0;
	
	ret = mpMem->Peek16bit(mReg.PtrAddr++);
	mReg.PtrAddr++;
	ret += offs;
	
	return ret;
}

/*
 *--------------------------------------------------------------------
 * Method:		LogicOpAcc()
 * Purpose:		Perform logical bitwise operation between memory
 *						location and Acc, result in Acc. Set flags.
 * Arguments:	addr - memory location
 *						logop - logical operation code: LOGOP_OR, LOGOP_AND,
 *																						LOGOP_EOR
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::LogicOpAcc(unsigned short addr, int logop)
{
	unsigned char val = 0;
	
	val = mpMem->Peek8bit(addr);
	
	CollapseAccQ();

	switch (logop) {
		case LOGOP_OR:
			mReg.Acc |= val;
			break;
		case LOGOP_AND:
			mReg.Acc &= val;
			break;
		case LOGOP_EOR:
			mReg.Acc ^= val;
			break;
		default:
			break;
	}

	SetFlags(mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		CompareOpAcc()
 * Purpose:		Subtract val from Acc, conditioning flags.
 *			Then, add val back into Acc, leaving flags alone.
 * Arguments:		val - val to compare
 *																					
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::CompareOpAcc(unsigned char val)
{
	if ((mReg.Flags & FLAGS_QUANTUM) && (mReg.isAccQ || mReg.isCarryQ)) {
		PrepareCarryQ();
		PrepareAccQ();
		qReg->DEC(val, REGS_ACC_Q, REG_LEN);
		qReg->CPhaseFlipIfLess(val, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q);
		if (mReg.Flags & FLAGS_ZERO) qReg->ZeroPhaseFlip(REGS_ACC_Q, REG_LEN);
		if (mReg.Flags & FLAGS_SIGN) qReg->Z(REGS_ACC_Q + REG_LEN - 1);
		qReg->INC(val, REGS_ACC_Q, REG_LEN);
	}
	else {
		CollapseAccQ();

		SetFlag((mReg.Acc >= val), FLAGS_CARRY);
		val = mReg.Acc - val;
		SetFlags(val);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		CompareOpIndX()
 * Purpose:		Subtract val from IndX, conditioning flags.
 *			Then, add val back into IndX, leaving flags alone.
 * Arguments:		val - val to compare
 *																					
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::CompareOpIndX(unsigned char val)
{	
	if ((mReg.Flags & FLAGS_QUANTUM) && (mReg.isXQ || mReg.isCarryQ)) {
		PrepareCarryQ();
		PrepareXQ();
		qReg->DEC(val, REGS_INDX_Q, REG_LEN);
		qReg->CPhaseFlipIfLess(val, REGS_INDX_Q, REG_LEN, FLAGS_CARRY_Q);
		qReg->Z(FLAGS_CARRY_Q);
		if (mReg.Flags & FLAGS_ZERO) qReg->ZeroPhaseFlip(REGS_INDX_Q, REG_LEN);
		if (mReg.Flags & FLAGS_SIGN) qReg->Z(REGS_INDX_Q + REG_LEN - 1);
		qReg->INC(val, REGS_INDX_Q, REG_LEN);
	}
	else {
		CollapseXQ();
		
		SetFlag((mReg.IndX >= val), FLAGS_CARRY);
		val = mReg.IndX - val;
		SetFlags(val);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		CompareOpIndY()
 * Purpose:		Subtract val from IndY, conditioning flags.
 *			Then, add val back into IndY, leaving flags alone.
 * Arguments:		val - val to compare
 *																					
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::CompareOpIndY(unsigned char val)
{	
	if ((mReg.Flags & FLAGS_QUANTUM) && (mReg.isYQ || mReg.isCarryQ)) {
		PrepareCarryQ();
		PrepareYQ();
		qReg->DEC(val, REGS_INDY_Q, REG_LEN);
		qReg->CPhaseFlipIfLess(val, REGS_INDY_Q, REG_LEN, FLAGS_CARRY_Q);
		qReg->Z(FLAGS_CARRY_Q);
		if (mReg.Flags & FLAGS_ZERO) qReg->ZeroPhaseFlip(REGS_INDY_Q, REG_LEN);
		if (mReg.Flags & FLAGS_SIGN) qReg->Z(REGS_INDY_Q + REG_LEN - 1);
		qReg->INC(val, REGS_INDY_Q, REG_LEN);
	}
	else {
		CollapseYQ();
		
		SetFlag((mReg.IndY >= val), FLAGS_CARRY);
		val = mReg.IndY - val;
		SetFlags(val);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		ComputeRelJump()
 * Purpose:		Compute new PC based on relative offset.
 * Arguments:	offs - relative offset [-128 ($80)..127 ($7F)]
 * Returns:		unsigned short - new PC (Program Counter).
 * NOTE:
 * Program Counter (http://www.6502.org/tutorials/6502opcodes.html#PC)
 * When the 6502 is ready for the next instruction it increments the 
 * program counter before fetching the instruction. Once it has the op
 * code, it increments the program counter by the length of the
 * operand, if any. This must be accounted for when calculating
 * branches or when pushing bytes to create a false return address
 * (i.e. jump table addresses are made up of addresses-1 when it is
 * intended to use an RTS rather than a JMP).
 * The program counter is loaded least signifigant byte first.
 * Therefore the most signifigant byte must be pushed first when
 * creating a false return address.
 * When calculating branches a forward branch of 6 skips the following
 * 6 bytes so, effectively the program counter points to the address
 * that is 8 bytes beyond the address of the branch opcode;
 * and a backward branch of $FA (256-6) goes to an address 4 bytes
 * before the branch instruction. 
 *--------------------------------------------------------------------
 */
unsigned short MKCpu::ComputeRelJump(unsigned char offs)
{
	// NOTE: mReg.PtrAddr (PC) must be at the next opcode (after branch instr.)
	// 				at this point.
	return ComputeRelJump(mReg.PtrAddr, offs);
}

/*
 *--------------------------------------------------------------------
 * Method:		ComputeRelJump()
 * Purpose:		Compute new address after branch based on relative
 *            offset.
 * Arguments:	addr - next opcode address (after branch instr.)
 *            offs - relative offset [-128 ($80)..127 ($7F)]
 * Returns:		unsigned short - new address for branch jump
 *--------------------------------------------------------------------
 */
unsigned short MKCpu::ComputeRelJump(unsigned short addr, unsigned char offs)
{
	unsigned short newpc = addr;
	
	if (offs < 0x80) {
		newpc += (unsigned short) offs;
	} else {  // use binary 2's complement instead of arithmetics
		newpc -= (unsigned short) ((unsigned char)(~offs + 1));
	}	
	
	return newpc;
}

/*
 *--------------------------------------------------------------------
 * Method:		Conv2Bcd()
 * Purpose:		Convert 16-bit unsigned number to 8-bit BCD
 *            representation.
 * Arguments:	v - 16-bit unsigned integer.
 * Returns:		byte representing BCD code of the 'v'.
 *--------------------------------------------------------------------
 */
unsigned char MKCpu::Conv2Bcd(unsigned short v)
{
   unsigned char arg8 = 0;
   arg8 = (unsigned char)((v/10) << 4);
   arg8 |= ((unsigned char)(v - (v/10)*10)) & 0x0F;
   return arg8;
}

/*
 *--------------------------------------------------------------------
 * Method:		Bcd2Num()
 * Purpose:		Convert 8-bit BCD code to a number.
 * Arguments:	v - BCD code.
 * Returns:		16-bit unsigned integer
 *--------------------------------------------------------------------
 */
unsigned short MKCpu::Bcd2Num(unsigned char v)
{
	unsigned short ret = 0;
	ret = 10 * ((v & 0xF0) >> 4) + (unsigned char)(v & 0x0F);
	return ret;
}

/*
 *--------------------------------------------------------------------
 * Method:		CheckFlag()
 * Purpose:		Check if given bit flag in CPU status register is set
 *            or not.
 * Arguments:	flag - byte with the bit to be checked set and other
 *                   bits not set.
 * Returns:		bool
 *--------------------------------------------------------------------
 */
bool MKCpu::CheckFlag(unsigned char flag)
{
	mReg.Flags &= ~FLAGS_CARRY;
	if (flag & FLAGS_CARRY) CollapseCarryQ();
	return ((mReg.Flags & flag) == flag);
}

/*
 *--------------------------------------------------------------------
 * Method:		SetFlag()
 * Purpose:		Set or unset CPU status flag.
 * Arguments:	set - if true, set flag, if false, unset flag.
 *            flag - a byte with a bit set on the position of flag
 *                   being altered and zeroes on other positions.
 * Returns:		n/a
 *--------------------------------------------------------------------
 */

void MKCpu::SetFlag(bool set, unsigned char flag)
{
	if (flag & FLAGS_CARRY) CollapseCarryQ();
	if (mReg.Flags & FLAGS_QUANTUM) { //quantum mode
		if (set) {
			if ((flag & FLAGS_ZERO) && (mReg.Flags & FLAGS_ZERO)) qReg->PhaseFlip();
			if ((flag & FLAGS_OVERFLOW) && (mReg.Flags & FLAGS_OVERFLOW)) qReg->PhaseFlip();
			if ((flag & FLAGS_SIGN) && (mReg.Flags & FLAGS_SIGN)) qReg->PhaseFlip();
		}
	}
	else { //classical mode
		if (set) {
			mReg.Flags |= flag;
		} else {
			mReg.Flags &= ~flag;
		}
	}

	if (flag & FLAGS_QUANTUM) {
		if (set) {
			mReg.Flags |= FLAGS_QUANTUM;
		}
		else {
			mReg.Flags &= ~FLAGS_QUANTUM;
		}
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		AddWithCarry()
 * Purpose:		Add Acc + Mem with Carry, update flags and Acc.
 * Arguments:	mem8 - memory argument (byte)
 * Returns:		byte value Acc + Mem + Carry
 *--------------------------------------------------------------------
 */
unsigned char MKCpu::AddWithCarry(unsigned char mem8)
{
	bool isQuantum = CheckFlag(FLAGS_QUANTUM) && (mReg.isAccQ || mReg.isCarryQ);

	if (isQuantum) { //quantum mode
		PrepareAccQ();
		PrepareCarryQ();
		if (CheckFlag(FLAGS_DEC)) {
			qReg->INCBCDC(mem8, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q);
		}
		else {
			if (mReg.Flags & FLAGS_OVERFLOW) {
				qReg->INCSC(mem8, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q);
			}
			else {
				qReg->INCC(mem8, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q);
			}
		}
		SetFlagsReg(REGS_ACC_Q);
	}
	else {
		//In classical mode, collapse the accumulator state when necessary.
		CollapseAccQ();
		CollapseCarryQ();
	}
	
	//Classical operation is always carried out to mirror the quantum register via Ehrenfest's theorem.

	// This algorithm was adapted from Frodo emulator code.
	unsigned short utmp16 = mReg.Acc + mem8 + ((mReg.Flags & FLAGS_CARRY) ? 1 : 0);
	if (mReg.Flags & FLAGS_DEC) {	// BCD mode
	
		unsigned short al = (mReg.Acc & 0x0F) + (mem8 & 0x0F) + ((mReg.Flags & FLAGS_CARRY) ? 1 : 0);
		if (al > 9) al += 6;
		unsigned short ah = (mReg.Acc >> 4) + (mem8 >> 4);
		if (al > 0x0F) ah++;
		if (!isQuantum) {
			SetFlag((utmp16 == 0), FLAGS_ZERO);
			SetFlag(((((ah << 4) ^ mReg.Acc) & 0x80) && !((mReg.Acc ^ mem8) & 0x80)), FLAGS_OVERFLOW);
		}
		if (ah > 9) ah += 6;
		if (!isQuantum) {
			SetFlag((ah > 0x0F), FLAGS_CARRY);
		}
		mReg.Acc = (ah << 4) | (al & 0x0f);
		if (!isQuantum) {
			SetFlag((mReg.Acc & FLAGS_SIGN) == FLAGS_SIGN, FLAGS_SIGN);
		}
	} else {	// binary mode
		if (!isQuantum) {
			SetFlag((utmp16 > 0xff), FLAGS_CARRY);
			SetFlag((!((mReg.Acc ^ mem8) & 0x80) && ((mReg.Acc ^ utmp16) & 0x80)),
							 FLAGS_OVERFLOW);
		}
		mReg.Acc = utmp16 & 0xFF;
		if (!isQuantum) {
			SetFlag((mReg.Acc == 0), FLAGS_ZERO);
			SetFlag((mReg.Acc & FLAGS_SIGN) == FLAGS_SIGN, FLAGS_SIGN);
		}
	}

	return mReg.Acc;
}

/*
 *--------------------------------------------------------------------
 * Method:		SubWithCarry()
 * Purpose:		Subtract Acc - Mem with Carry, update flags and Acc.
 * Arguments:	mem8 - memory argument (byte)
 * Returns:		byte value Acc - Mem - Carry
 *--------------------------------------------------------------------
 */
unsigned char MKCpu::SubWithCarry(unsigned char mem8)
{
	bool isQuantum = CheckFlag(FLAGS_QUANTUM) && (mReg.isAccQ || mReg.isCarryQ);

	if (isQuantum) { //quantum mode
		PrepareAccQ();
		PrepareCarryQ();
		if (CheckFlag(FLAGS_DEC)) {
			qReg->DECBCDC(mem8, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q);
		}
		else {
			if (mReg.Flags & FLAGS_OVERFLOW) {
				qReg->DECSC(mem8, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q);
			}
			else {
				qReg->DECC(mem8, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q);
			}
		}
		SetFlagsReg(REGS_ACC_Q);
	}
	else {
		//In classical mode, collapse the accumulator state when necessary.
		CollapseAccQ();
		CollapseCarryQ();
	}

	//Classical operation is always carried out to mirror the quantum register via Ehrenfest's theorem.

	// This algorithm was adapted from Frodo emulator code.
	unsigned short utmp16 = mReg.Acc - mem8 - ((mReg.Flags & FLAGS_CARRY) ? 0 : 1);
	if (mReg.Flags & FLAGS_DEC) {	// BCD mode
	
		unsigned char al = (mReg.Acc & 0x0F) - (mem8 & 0x0F) - ((mReg.Flags & FLAGS_CARRY) ? 0 : 1);
		unsigned char ah = (mReg.Acc >> 4) - (mem8 >> 4);
		if (al & 0x10) {
			al -= 6; ah--;
		}
		if (ah & 0x10) ah -= 6;
		if (!isQuantum) {
			SetFlag((utmp16 < 0x100), FLAGS_CARRY);
			SetFlag(((mReg.Acc ^ utmp16) & 0x80) && ((mReg.Acc ^ mem8) & 0x80), FLAGS_OVERFLOW);
			SetFlag((utmp16 == 0), FLAGS_ZERO);
		}
		mReg.Acc = (ah << 4) | (al & 0x0f);
		if (!isQuantum) {
			SetFlag((mReg.Acc & FLAGS_SIGN) == FLAGS_SIGN, FLAGS_SIGN);
		}
		
	} else { // binary mode
		if (!isQuantum) {
			SetFlag((utmp16 < 0x100), FLAGS_CARRY);
			SetFlag(((mReg.Acc ^ utmp16) & 0x80) && ((mReg.Acc ^ mem8) & 0x80), FLAGS_OVERFLOW);
		}
		mReg.Acc = utmp16 & 0xFF;
		if (!isQuantum) {
			SetFlag((mReg.Acc == 0), FLAGS_ZERO);
			SetFlag((mReg.Acc & FLAGS_SIGN) == FLAGS_SIGN, FLAGS_SIGN);
		}
	
	}

	return mReg.Acc;
}


/*
 *--------------------------------------------------------------------
 * Method:		~MKCpu()
 * Purpose:		Class destructor.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
MKCpu::~MKCpu()
{
	if (mLocalMem) {
		if (NULL != mpMem)
			delete mpMem;
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		GetAddrWithMode()
 * Purpose:		Get address of the argument with specified mode.
 *            Increment PC.
 * Arguments:	mode - code of the addressing mode, see eAddrModes.
 * Returns:		16-bit address
 *--------------------------------------------------------------------
 */
unsigned short MKCpu::GetAddrWithMode(int mode)
{
	unsigned short arg16 = 0, tmp = 0;
	
	mReg.PageBoundary = false;
	mReg.LastAddrMode = mode;
	switch (mode) {
		
		case ADDRMODE_IMM:
			arg16 = mReg.PtrAddr++;
			break;
			
		case ADDRMODE_ABS:
			mReg.LastArg = arg16 = GetArg16(0);
			break;
			
		case ADDRMODE_ZP:
			mReg.LastArg = arg16 = (unsigned short) mpMem->Peek8bit(mReg.PtrAddr++);
			break;
			
		case ADDRMODE_IMP:
			// DO NOTHING - implied mode operates on internal CPU register
			break;
			
		case ADDRMODE_IND:
			mReg.LastArg = arg16 = mpMem->Peek16bit(mReg.PtrAddr++);
			arg16 = mpMem->Peek16bit(arg16);
			break;
	
		case ADDRMODE_ABX:
			mReg.LastArg = tmp = GetArg16(0);
			arg16 = tmp;
			mReg.PageBoundary = PageBoundary(tmp, arg16 + 255);
			break;
			
		case ADDRMODE_ABY:
			mReg.LastArg = tmp = GetArg16(0);
			arg16 = tmp + mReg.IndY;
			mReg.PageBoundary = PageBoundary(tmp, arg16);
			break;
			
		case ADDRMODE_ZPX:
			mReg.LastArg = arg16 = mpMem->Peek8bit(mReg.PtrAddr++);
			break;
			
		case ADDRMODE_ZPY:
			mReg.LastArg = arg16 = mpMem->Peek8bit(mReg.PtrAddr++);
			arg16 = (arg16 + mReg.IndY) & 0xFF;
			break;
			
		case ADDRMODE_IZX:
			mReg.LastArg = arg16 = mpMem->Peek8bit(mReg.PtrAddr++);
			arg16 = arg16 & 0xFF;
			arg16 = mpMem->Peek16bit(arg16);			
			break;
			
		case ADDRMODE_IZY:
			mReg.LastArg = arg16 = mpMem->Peek8bit(mReg.PtrAddr++);
			tmp = mpMem->Peek16bit(arg16);
			arg16 = tmp + mReg.IndY;
			mReg.PageBoundary = PageBoundary(tmp, arg16);
			break;
			
		case ADDRMODE_REL:
			mReg.LastArg = arg16 = ComputeRelJump(mpMem->Peek8bit(mReg.PtrAddr++));
			mReg.PageBoundary = PageBoundary(mReg.PtrAddr, arg16);
			break;
			
		case ADDRMODE_ACC:
			// DO NOTHING - acc mode operates on internal CPU register
			break;
			
		default:
			throw MKGenException("ERROR: Wrong addressing mode!");
			break;
	}
	
	return arg16;
}

/*
 *--------------------------------------------------------------------
 * Method:		GetArgWithMode()
 * Purpose:		Get argument from address with specified mode.
 * Arguments:	addr - address in memory
 *            mode - code of the addressing mode, see eAddrModes.
 * Returns:		argument
 *--------------------------------------------------------------------
 */
unsigned short MKCpu::GetArgWithMode(unsigned short addr, int mode)
{
	unsigned short arg16 = 0;
	
	switch (mode) {
		
		case ADDRMODE_IMM:
			arg16 = mpMem->Peek8bit(addr);
			break;
			
		case ADDRMODE_ABS:
			arg16 = mpMem->Peek16bit(addr);
			break;
			
		case ADDRMODE_ZP:
			arg16 = (unsigned short) mpMem->Peek8bit(addr);
			break;
			
		case ADDRMODE_IMP:
			// DO NOTHING - implied mode operates on internal CPU register
			break;
			
		case ADDRMODE_IND:
			arg16 = mpMem->Peek16bit(addr);
			break;
	
		case ADDRMODE_ABX:
			arg16 = mpMem->Peek16bit(addr);
			break;
			
		case ADDRMODE_ABY:
			arg16 = mpMem->Peek16bit(addr);
			break;
			
		case ADDRMODE_ZPX:
			arg16 = mpMem->Peek8bit(addr);
			break;
			
		case ADDRMODE_ZPY:
			arg16 = mpMem->Peek8bit(addr);
			break;
			
		case ADDRMODE_IZX:
			arg16 = mpMem->Peek8bit(addr);
			break;
			
		case ADDRMODE_IZY:
			arg16 = mpMem->Peek8bit(addr);
			break;
			
		case ADDRMODE_REL:
			arg16 = ComputeRelJump(addr+1, mpMem->Peek8bit(addr));
			break;
			
		case ADDRMODE_ACC:
			// DO NOTHING - acc mode operates on internal CPU register
			break;
			
		default:
			break;
	}
	
	return arg16;
}

/*
 *--------------------------------------------------------------------
 * Method:		Disassemble()
 * Purpose:		Disassemble op-code exec. history item.
 * Arguments:	histit - pointer to OpCodeHistItem type
 * Returns:		0
 *--------------------------------------------------------------------
 */
unsigned short MKCpu::Disassemble(OpCodeHistItem *histit)
{
	char sArg[40];
	char sFmt[20];

	strcpy(sFmt, "%s ");
	strcat(sFmt, mArgFmtTbl[histit->LastAddrMode].c_str());
	sprintf(sArg, sFmt, 
						((mOpCodesMap[(eOpCodes)histit->LastOpCode]).amf.length() > 0 
							? (mOpCodesMap[(eOpCodes)histit->LastOpCode]).amf.c_str() : "???"),
						histit->LastArg);
	for (unsigned int i=0; i<strlen(sArg); i++) sArg[i] = toupper(sArg[i]);
	histit->LastInstr = sArg;
	
	return 0;
}

/*
 *--------------------------------------------------------------------
 * Method:		Disassemble()
 * Purpose:		Disassemble instruction in memory.
 * Arguments:	addr - address in memory
 *            instrbuf - pointer to a character buffer, this is where
 *                       instructions will be disassembled
 * Returns:		unsigned short - address of next instruction
 *--------------------------------------------------------------------
 */
unsigned short MKCpu::Disassemble(unsigned short opcaddr, char *instrbuf)
{
	char sArg[DISS_BUF_SIZE] = {0};
	char sBuf[10] = {0};
	char sFmt[40] = {0};
	int addr = opcaddr;
	int opcode = -1;
	int addrmode = -1;

	opcode = mpMem->Peek8bitImg(addr++);
	addrmode = (mOpCodesMap[(eOpCodes)opcode]).amf.length() > 0
							? (mOpCodesMap[(eOpCodes)opcode]).addrmode : -1;
							
  if (addrmode < 0 || NULL == instrbuf) return 0;
  switch (mAddrModesLen[addrmode])
  {
  	case 2:
  		sprintf(sBuf, "$%02x    ", mpMem->Peek8bitImg(addr));
  		break;
  		
  	case 3:
  		sprintf(sBuf, "$%02x $%02x", mpMem->Peek8bitImg(addr), 
  						mpMem->Peek8bitImg(addr+1));
  		break;
  		  	
  	default:
  		strcpy(sBuf, "       ");
  		break;
	}
	strcpy(sFmt, "$%04x: $%02x %s   %s ");
	strcat(sFmt, mArgFmtTbl[addrmode].c_str());
	sprintf(sArg, sFmt, opcaddr, mpMem->Peek8bitImg(opcaddr), sBuf,
								((mOpCodesMap[(eOpCodes)opcode]).amf.length() > 0 
											? (mOpCodesMap[(eOpCodes)opcode]).amf.c_str() : "???"),
								GetArgWithMode(addr,addrmode));
	for (unsigned int i=0; i<strlen(sArg); i++) sArg[i] = toupper(sArg[i]);
	strcpy(instrbuf, sArg);
	
	return opcaddr + mAddrModesLen[addrmode];
}

/*
 *--------------------------------------------------------------------
 * Method:		PageBoundary()
 * Purpose:		Detect if page boundary was crossed.
 * Arguments:	startaddr - unsigned short, starting address
 *            endaddr - unsigned short, end address
 * Returns:		bool - true if memory page changes between startaddr
 *                   and endaddr
 *--------------------------------------------------------------------
 */
bool MKCpu::PageBoundary(unsigned short startaddr, unsigned short endaddr)
{
	return (startaddr != (endaddr & 0xFF00));
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeBrk()
 * Purpose:		Execure BRK opcode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeBrk()
{
	unsigned short arg16 = 0;
	// software interrupt, Implied ($00 : BRK)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (!CheckFlag(FLAGS_IRQ)) {	// only if IRQ not masked
		arg16 = 0x100;
		arg16 += mReg.PtrStack--;
		// Note that BRK is really a 2-bytes opcode. Each BRK opcode should be
		// padded by extra byte, because the return address put on stack is PC + 1
		// (where PC is the next address after BRK).
		// That means the next opcode after BRK will not be executed upon return
		// from interrupt, but the next after that will be.
		// In case of hardware IRQ, just put current address on stack.
		if (!mReg.IrqPending) mReg.PtrAddr++;
		// HI(PC+1) - HI part of next instr. addr. + 1
		mpMem->Poke8bit(arg16, (unsigned char) (((mReg.PtrAddr) & 0xFF00) >> 8));
		arg16 = 0x100;
		arg16 += mReg.PtrStack--;
		// LO(PC+1) - LO part of next instr. addr. + 1
		mpMem->Poke8bit(arg16, (unsigned char) ((mReg.PtrAddr) & 0x00FF));
		arg16 = 0x100;
		arg16 += mReg.PtrStack--;
		// The BRK flag that goes on stack is set in case of BRK
		//  or cleared in case of IRQ.
		SetFlag(!mReg.IrqPending, FLAGS_BRK);
		mpMem->Poke8bit(arg16, mReg.Flags);
		// The BRK flag that remains in CPU status is unchanged, so unset after
		// putting on stack.
		SetFlag(false, FLAGS_BRK);
		//mReg.SoftIrq = true;
		// Load IRQ/BRK vector into the PC.
		mReg.PtrAddr = mpMem->Peek16bit(0xFFFE);
		SetFlag(true,FLAGS_IRQ);	// block interrupts (RTI will clear it)
	} else {
		mReg.PtrAddr++;
	}
	if (mReg.IrqPending) mReg.IrqPending = false;	// let the next request come
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeNop()
 * Purpose:		Execute NOP opcode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeNop()
{
	// NO oPeration, Implied ($EA : NOP)
	mReg.LastAddrMode = ADDRMODE_IMP;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdaIzx()
 * Purpose:		Execute LDA opcode, IZX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdaIzx()
{
	unsigned short arg16 = 0;
	// LoaD Accumulator, Indexed Indirect ($A1 arg : LDA (arg,X) 
	// ;arg=0..$FF), MEM=&(arg+X)
	arg16 = GetAddrWithMode(ADDRMODE_IZX);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isXQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit((arg16 + i) & 0xFF);
		}
		PrepareAccQ();
		mReg.Acc = qReg->IndexedLDA(REGS_INDX_Q, REG_LEN, REGS_ACC_Q, REG_LEN, toLoad);
	}
	else {
		arg16 = (arg16 + mReg.IndX) & 0xFF;
		CollapseAccQ();
		mReg.Acc = mpMem->Peek8bit(arg16);
	}
	if (mReg.isAccQ) {
		SetFlagsReg(REGS_ACC_Q);
	}
	else {
		SetFlags(mReg.Acc);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdaZp()
 * Purpose:		Execute LDA opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdaZp()
{
	// LoaD Accumulator, Zero Page ($A5 arg : LDA arg ;arg=0..$FF),
	// MEM=arg
	CollapseAccQ();
	mReg.Acc = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_ZP));
	SetFlags(mReg.Acc);			
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdaImm()
 * Purpose:		Execute LDA opcode, IMM addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdaImm()
{
	// LoaD Accumulator, Immediate ($A9 arg : LDA #arg ;arg=0..$FF),
	// MEM=PC+1
	CollapseAccQ();
	mReg.Acc = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_IMM));
	mReg.LastArg = mReg.Acc;
	SetFlags(mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdaAbs()
 * Purpose:		Execute LDA opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdaAbs()
{
	unsigned short arg16 = 0;
	// LoaD Accumulator, Absolute ($AD addrlo addrhi : LDA addr
	// ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	CollapseAccQ();
	mReg.Acc = mpMem->Peek8bit(arg16);
	SetFlags(mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdaIzy()
 * Purpose:		Execute LDA opcode, IZY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdaIzy()
{
	unsigned short arg16 = 0;
	// LoaD Accumulator, Indirect Indexed ($B1 arg : LDA (arg),Y
	// ;arg=0..$FF), MEM=&arg+Y	
	arg16 = GetAddrWithMode(ADDRMODE_IZY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	CollapseAccQ();
	mReg.Acc = mpMem->Peek8bit(arg16);
	SetFlags(mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdaZpx()
 * Purpose:		Execute LDA opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdaZpx()
{
	unsigned short arg16 = 0;
	// LoaD Accumulator, Zero Page Indexed, X ($B5 arg : LDA arg,X
	// ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isXQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit((arg16 + i) & 0xFF);
		}
		PrepareAccQ();
		mReg.Acc = qReg->IndexedLDA(REGS_INDX_Q, REG_LEN, REGS_ACC_Q, REG_LEN, toLoad);
	}
	else {
		arg16 = (arg16 + mReg.IndX) & 0xFF;
		CollapseAccQ();
		mReg.Acc = mpMem->Peek8bit(arg16);
	}
	if (mReg.isAccQ) {
		SetFlagsReg(REGS_ACC_Q);
	}
	else {
		SetFlags(mReg.Acc);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdaAby()
 * Purpose:		Execute LDA opcode, ABY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdaAby()
{
	unsigned short arg16 = 0;
	// LoaD Accumulator, Absolute Indexed, Y
	// ($B9 addrlo addrhi : LDA addr,Y ;addr=0..$FFFF), MEM=addr+Y
	arg16 = GetAddrWithMode(ADDRMODE_ABY);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isYQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit(arg16 + i);
		}
		PrepareAccQ();
		mReg.Acc = qReg->IndexedLDA(REGS_INDY_Q, REG_LEN, REGS_ACC_Q, REG_LEN, toLoad);
	}
	else {
		arg16 = arg16 + mReg.IndY;
		CollapseAccQ();
		mReg.Acc = mpMem->Peek8bit(arg16);
	}
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	if (mReg.isAccQ) {
		SetFlagsReg(REGS_ACC_Q);
	}
	else {
		SetFlags(mReg.Acc);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdaAbx()
 * Purpose:		Execute LDA opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdaAbx()
{
	unsigned short arg16 = 0;
	// LoaD Accumulator, Absolute Indexed, X
	// ($BD addrlo addrhi : LDA addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isXQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit(arg16 + i);
		}
		PrepareAccQ();
		mReg.Acc = qReg->IndexedLDA(REGS_INDX_Q, REG_LEN, REGS_ACC_Q, REG_LEN, toLoad);
	}
	else {
		arg16 = arg16 + mReg.IndX;
		CollapseAccQ();
		mReg.Acc = mpMem->Peek8bit(arg16);
	}
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	if (mReg.isAccQ) {
		SetFlagsReg(REGS_ACC_Q);
	}
	else {
		SetFlags(mReg.Acc);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdxImm()
 * Purpose:		Execute LDX opcode, IMM addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdxImm()
{
	// LoaD X register, Immediate ($A2 arg : LDX #arg ;arg=0..$FF),
	// MEM=PC+1
	unsigned char toX = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_IMM));
	CollapseXQ();
	mReg.IndX = toX;
	mReg.LastArg = mReg.IndX;
	SetFlags(mReg.IndX);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdxZp()
 * Purpose:		Execute LDX opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdxZp()
{
	// LoaD X register, Zero Page ($A6 arg : LDX arg ;arg=0..$FF),
	// MEM=arg
	unsigned char toX = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_ZP));
	CollapseXQ();
	mReg.IndX = toX;
	SetFlags(mReg.IndX);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdxAbs()
 * Purpose:		Execute LDX opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdxAbs()
{
	unsigned short arg16 = 0;
	// LoaD X register, Absolute
	// ($AE addrlo addrhi : LDX addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	unsigned char toX = mpMem->Peek8bit(arg16);
	CollapseXQ();
	mReg.IndX = toX;
	SetFlags(mReg.IndX);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdxZpy()
 * Purpose:		Execute LDX opcode, ZPY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdxZpy()
{
	unsigned short arg16 = 0;
	// LoaD X register, Zero Page Indexed, Y
	// ($B6 arg : LDX arg,Y ;arg=0..$FF), MEM=arg+Y
	arg16 = GetAddrWithMode(ADDRMODE_ZPY);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isYQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit((arg16 + i) & 0xFF);
		}
		PrepareXQ();
		mReg.IndX = qReg->IndexedLDA(REGS_INDY_Q, REG_LEN, REGS_INDX_Q, REG_LEN, toLoad);
	}
	else {
		arg16 = (arg16 + mReg.IndX) & 0xFF;
		CollapseXQ();
		mReg.IndX = mpMem->Peek8bit(arg16);
	}
	if (mReg.isXQ) {
		SetFlagsReg(REGS_INDX_Q);
	}
	else {
		SetFlags(mReg.IndX);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdxAby()
 * Purpose:		Execute LDX opcode, ABY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdxAby()
{
	unsigned short arg16 = 0;
	// LoaD X register, Absolute Indexed, Y
	// ($BE addrlo addrhi : LDX addr,Y ;addr=0..$FFFF), MEM=addr+Y
	arg16 = GetAddrWithMode(ADDRMODE_ABY);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isYQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit(arg16 + i);
		}
		PrepareXQ();
		mReg.IndX = qReg->IndexedLDA(REGS_INDY_Q, REG_LEN, REGS_INDX_Q, REG_LEN, toLoad);
	}
	else {
		arg16 = arg16 + mReg.IndY;
		CollapseXQ();
		mReg.IndX = mpMem->Peek8bit(arg16);
	}
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	if (mReg.isXQ) {
		SetFlagsReg(REGS_INDX_Q);
	}
	else {
		SetFlags(mReg.IndX);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdyImm()
 * Purpose:		Execute LDY opcode, IMM addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdyImm()
{
	// LoaD Y register, Immediate ($A0 arg : LDY #arg ;arg=0..$FF),
	// MEM=PC+1
	CollapseYQ();
	mReg.IndY = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_IMM));
	mReg.LastArg = mReg.IndY;
	SetFlags(mReg.IndY);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdyZp()
 * Purpose:		Execute LDY opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdyZp()
{
	// LoaD Y register, Zero Page ($A4 arg : LDY arg ;arg=0..$FF),
	// MEM=arg
	CollapseYQ();
	mReg.IndY = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_ZP));
	SetFlags(mReg.IndY);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdyAbs()
 * Purpose:		Execute LDY opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdyAbs()
{
	// LoaD Y register, Absolute
	// ($AC addrlo addrhi : LDY addr ;addr=0..$FFFF), MEM=addr
	CollapseYQ();
	mReg.IndY = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_ABS));
	SetFlags(mReg.IndY);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdyZpx()
 * Purpose:		Execute LDY opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdyZpx()
{
	unsigned short arg16 = 0;
	// LoaD Y register, Zero Page Indexed, X
	// ($B4 arg : LDY arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isXQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit((arg16 + i) & 0xFF);
		}
		PrepareYQ();
		mReg.IndY = qReg->IndexedLDA(REGS_INDX_Q, REG_LEN, REGS_INDY_Q, REG_LEN, toLoad);
	}
	else {
		arg16 = (arg16 + mReg.IndY) & 0xFF;
		CollapseYQ();
		mReg.IndY = mpMem->Peek8bit(arg16);
	}
	if (mReg.isYQ) {
		SetFlagsReg(REGS_INDY_Q);
	}
	else {
		SetFlags(mReg.IndY);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLdyAbx()
 * Purpose:		Execute LDY opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLdyAbx()
{
	unsigned short arg16 = 0;
	// LoaD Y register, Absolute Indexed, X
	// ($BC addrlo addrhi : LDY addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isXQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit(arg16 + i);
		}
		PrepareYQ();
		mReg.IndY = qReg->IndexedLDA(REGS_INDX_Q, REG_LEN, REGS_INDY_Q, REG_LEN, toLoad);
	}
	else {
		arg16 = arg16 + mReg.IndX;
		CollapseYQ();
		mReg.IndY = mpMem->Peek8bit(arg16);
	}
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	if (mReg.isYQ) {
		SetFlagsReg(REGS_INDY_Q);
	}
	else {
		SetFlags(mReg.IndY);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeTax()
 * Purpose:		Execute TAX opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeTax()
{
	// Transfer A to X, Implied ($AA : TAX)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.isAccQ) {
		qReg->SetReg(REGS_INDX_Q, REG_LEN, 0);
		qReg->CNOT(REGS_ACC_Q, REGS_INDX_Q, REG_LEN);
		mReg.isXQ = true;
	}
	else {
		CollapseXQ();
	}
	mReg.IndX = mReg.Acc;
	if (mReg.isXQ) {
		SetFlagsReg(REGS_INDX_Q);
	}
	else {
		SetFlags(mReg.IndX);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeTay()
 * Purpose:		Execute TAY opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeTay()
{
	// Transfer A to Y, Implied ($A8 : TAY)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.isAccQ) {
		qReg->SetReg(REGS_INDY_Q, REG_LEN, 0);
		qReg->CNOT(REGS_ACC_Q, REGS_INDY_Q, REG_LEN);
		mReg.isYQ = true;
	}
	else {
		CollapseYQ();
	}
	mReg.IndY = mReg.Acc;
	if (mReg.isYQ) {
		SetFlagsReg(REGS_INDY_Q);
	}
	else {
		SetFlags(mReg.IndY);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeTxa()
 * Purpose:		Execute TXA opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeTxa()
{
	// Transfer X to A, Implied ($8A : TXA)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.isXQ) {
		qReg->SetReg(REGS_ACC_Q, REG_LEN, 0);
		qReg->CNOT(REGS_INDX_Q, REGS_ACC_Q, REG_LEN);
		mReg.isAccQ = true;
	}
	else {
		CollapseAccQ();
	}
	mReg.Acc = mReg.IndX;
	if (mReg.isAccQ) {
		SetFlagsReg(REGS_ACC_Q);
	}
	else {
		SetFlags(mReg.Acc);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeTya()
 * Purpose:		Execute TYA opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeTya()
{
	// Transfer Y to A, Implied ($98 : TYA)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.isYQ) {
		qReg->SetReg(REGS_ACC_Q, REG_LEN, 0);
		qReg->CNOT(REGS_INDY_Q, REGS_ACC_Q, REG_LEN);
		mReg.isAccQ = true;
	}
	else {
		CollapseAccQ();
	}
	mReg.Acc = mReg.IndY;
	if (mReg.isAccQ) {
		SetFlagsReg(REGS_ACC_Q);
	}
	else {
		SetFlags(mReg.Acc);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeTsx()
 * Purpose:		Execute TSX opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeTsx()
{
	// Transfer Stack ptr to X, Implied ($BA : TSX)
	mReg.LastAddrMode = ADDRMODE_IMP;
	CollapseXQ();
	mReg.IndX = mReg.PtrStack;
	SetFlags(mReg.IndX);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeTxs()
 * Purpose:		Execute TXS opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeTxs()
{
	// Transfer X to Stack ptr, Implied ($9A : TXS)
	mReg.LastAddrMode = ADDRMODE_IMP;
	CollapseXQ();
	mReg.PtrStack = mReg.IndX;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStaIzx()
 * Purpose:		Execute STA opcode, IZX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStaIzx()
{
	unsigned short arg16 = 0;
	// STore Accumulator, Indexed Indirect
	// ($81 arg : STA (arg,X) ;arg=0..$FF), MEM=&(arg+X)
	arg16 = GetAddrWithMode(ADDRMODE_IZX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	CollapseAccQ();
	mpMem->Poke8bit(arg16, mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStaZp()
 * Purpose:		Execute STA opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStaZp()
{
	unsigned short arg16 = 0;
	// STore Accumulator, Zero Page ($85 arg : STA arg ;arg=0..$FF),
	// MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	CollapseAccQ();
	mpMem->Poke8bit(arg16, mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStaAbs()
 * Purpose:		Execute STA opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStaAbs()
{
	unsigned short arg16 = 0;
	// STore Accumulator, Absolute
	// ($8D addrlo addrhi : STA addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	CollapseAccQ();
	mpMem->Poke8bit(arg16, mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStaIzy()
 * Purpose:		Execute STA opcode, IZY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStaIzy()
{
	unsigned short arg16 = 0;
	// STore Accumulator, Indirect Indexed
	// ($91 arg : STA (arg),Y ;arg=0..$FF), MEM=&arg+Y
	arg16 = GetAddrWithMode(ADDRMODE_IZY);
	CollapseAccQ();
	mpMem->Poke8bit(arg16, mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStaZpx()
 * Purpose:		Execute STA opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStaZpx()
{
	unsigned short arg16 = 0;
	// STore Accumulator, Zero Page Indexed, X
	// ($95 arg : STA arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	CollapseAccQ();
	mpMem->Poke8bit(arg16, mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStaAby()
 * Purpose:		Execute STA opcode, ABY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStaAby()
{
	unsigned short arg16 = 0;
	// STore Accumulator, Absolute Indexed, Y
	// ($99 addrlo addrhi : STA addr,Y ;addr=0..$FFFF), MEM=addr+Y
	arg16 = GetAddrWithMode(ADDRMODE_ABY);
	CollapseAccQ();
	mpMem->Poke8bit(arg16, mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStaAbx()
 * Purpose:		Execute STA opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStaAbx()
{
	unsigned short arg16 = 0;
	// STore Accumulator, Absolute Indexed, X
	// ($9D addrlo addrhi : STA addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX); 
	CollapseXQ();
	arg16 += mReg.IndX;
	CollapseAccQ();
	mpMem->Poke8bit(arg16, mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStxZp()
 * Purpose:		Execute STX opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStxZp()
{
	unsigned short arg16 = 0;
	// STore X register, Zero Page ($86 arg : STX arg ;arg=0..$FF),
	// MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	CollapseXQ();
	mpMem->Poke8bit(arg16, mReg.IndX);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStxAbs()
 * Purpose:		Execute STX opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStxAbs()
{
	unsigned short arg16 = 0;
	// STore X register, Absolute
	// ($8E addrlo addrhi : STX addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	CollapseXQ();
	mpMem->Poke8bit(arg16, mReg.IndX);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStxZpy()
 * Purpose:		Execute STX opcode, ZPY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStxZpy()
{
	unsigned short arg16 = 0;
	// STore X register, Zero Page Indexed, Y
	// ($96 arg : STX arg,Y ;arg=0..$FF), MEM=arg+Y
	arg16 = GetAddrWithMode(ADDRMODE_ZPY);
	CollapseXQ();
	mpMem->Poke8bit(arg16, mReg.IndX);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStyZp()
 * Purpose:		Execute STY opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStyZp()
{
	unsigned short arg16 = 0;
	// STore Y register, Zero Page ($84 arg : STY arg ;arg=0..$FF),
	// MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	CollapseYQ();
	mpMem->Poke8bit(arg16, mReg.IndY);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStyAbs()
 * Purpose:		Execute STY opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStyAbs()
{
	unsigned short arg16 = 0;
	// STore Y register, Absolute
	// ($8C addrlo addrhi : STY addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	CollapseYQ();
	mpMem->Poke8bit(arg16, mReg.IndY);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeStyZpx()
 * Purpose:		Execute STY opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeStyZpx()
{
	unsigned short arg16 = 0;
	// STore Y register, Zero Page Indexed, X
	// ($94 arg : STY arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	CollapseYQ();
	mpMem->Poke8bit(arg16, mReg.IndY);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeBneRel()
 * Purpose:		Execute BNE opcode, REL addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeBneRel()
{
	unsigned short arg16 = 0;
	// Branch on Not Equal, Relative ($D0 signoffs : BNE signoffs
	// ;signoffs=0..$FF [-128 ($80)..127 ($7F)])
  	arg16 = GetAddrWithMode(ADDRMODE_REL);
	if (!CheckFlag(FLAGS_ZERO)) {
		mReg.CyclesLeft += (mReg.PageBoundary ? 2 : 1);
		mReg.PtrAddr = arg16;
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeBeqRel()
 * Purpose:		Execute BEQ opcode, REL addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeBeqRel()
{
	unsigned short arg16 = 0;
	// Branch on EQual, Relative ($F0 signoffs : BEQ signoffs
	// ;signoffs=0..$FF [-128 ($80)..127 ($7F)])
	arg16 = GetAddrWithMode(ADDRMODE_REL);
	if (CheckFlag(FLAGS_ZERO)) {
		mReg.CyclesLeft += (mReg.PageBoundary ? 2 : 1);
		mReg.PtrAddr = arg16;
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeBplRel()
 * Purpose:		Execute BPL opcode, REL addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeBplRel()
{
	unsigned short arg16 = 0;
	// Branch on PLus, Relative ($10 signoffs : BPL signoffs
	// ;signoffs=0..$FF [-128 ($80)..127 ($7F)])
	arg16 = GetAddrWithMode(ADDRMODE_REL);
	if (!CheckFlag(FLAGS_SIGN)) {
		mReg.CyclesLeft += (mReg.PageBoundary ? 2 : 1);
		mReg.PtrAddr = arg16;
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeBmiRel()
 * Purpose:		Execute BMI opcode, REL addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeBmiRel()
{
	unsigned short arg16 = 0;
	// Branch on MInus, Relative ($30 signoffs : BMI signoffs
	// ;signoffs=0..$FF [-128 ($80)..127 ($7F)])
	arg16 = GetAddrWithMode(ADDRMODE_REL);
	if (CheckFlag(FLAGS_SIGN)) {
		mReg.CyclesLeft += (mReg.PageBoundary ? 2 : 1);
		mReg.PtrAddr = arg16;
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeBvcRel()
 * Purpose:		Execute BVC opcode, REL addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeBvcRel()
{
	unsigned short arg16 = 0;
	// Branch on oVerflow Clear, Relative ($50 signoffs : BVC signoffs
	// ;signoffs=0..$FF [-128 ($80)..127 ($7F)])
	arg16 = GetAddrWithMode(ADDRMODE_REL);
	if (!CheckFlag(FLAGS_OVERFLOW)) {
		mReg.CyclesLeft += (mReg.PageBoundary ? 2 : 1);
		mReg.PtrAddr = arg16;
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeBvsRel()
 * Purpose:		Execute BVS opcode, REL addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeBvsRel()
{
	unsigned short arg16 = 0;
	// Branch on oVerflow Set, Relative ($70 signoffs : BVS signoffs
	// ;signoffs=0..$FF [-128 ($80)..127 ($7F)])
	arg16 = GetAddrWithMode(ADDRMODE_REL);
	if (CheckFlag(FLAGS_OVERFLOW)) {
		mReg.CyclesLeft += (mReg.PageBoundary ? 2 : 1);
		mReg.PtrAddr = arg16;
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeBccRel()
 * Purpose:		Execute BCC opcode, REL addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeBccRel()
{
	unsigned short arg16 = 0;
	// Branch on Carry Clear, Relative ($90 signoffs : BCC signoffs
	// ;signoffs=0..$FF [-128 ($80)..127 ($7F)])
	arg16 = GetAddrWithMode(ADDRMODE_REL);
	if (!CheckFlag(FLAGS_CARRY)) {
		mReg.CyclesLeft += (mReg.PageBoundary ? 2 : 1);
		mReg.PtrAddr = arg16;
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeBcsRel()
 * Purpose:		Execute BCS opcode, REL addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeBcsRel()
{
	unsigned short arg16 = 0;
	// Branch on Carry Set, Relative ($B0 signoffs : BCS signoffs
	// ;signoffs=0..$FF [-128 ($80)..127 ($7F)])
	arg16 = GetAddrWithMode(ADDRMODE_REL);
	if (CheckFlag(FLAGS_CARRY)) {
		mReg.CyclesLeft += (mReg.PageBoundary ? 2 : 1);
		mReg.PtrAddr = arg16;
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeIncZp()
 * Purpose:		Execute INC opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeIncZp()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// INCrement memory, Zero Page ($E6 arg : INC arg ;arg=0..$FF),
	// MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	arg8 = mpMem->Peek8bit(arg16) + 1;
	mpMem->Poke8bit(arg16, arg8);
	SetFlags(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeIncAbs()
 * Purpose:		Execute INC opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeIncAbs()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// INCrement memory, Absolute
	// ($EE addrlo addrhi : INC addr ;addr=0..$FFFF), MEM=addr	
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	arg8 = mpMem->Peek8bit(arg16) + 1;
	mpMem->Poke8bit(arg16, arg8);
	SetFlags(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeIncZpx()
 * Purpose:		Execute INC opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeIncZpx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// INCrement memory, Zero Page Indexed, X
	// ($F6 arg : INC arg,X ;arg=0..$FF), MEM=arg+X	
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	arg8 = mpMem->Peek8bit(arg16) + 1;
	mpMem->Poke8bit(arg16, arg8);
	SetFlags(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeIncAbx()
 * Purpose:		Execute INC opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeIncAbx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// INCrement memory, Absolute Indexed, X
	// ($FE addrlo addrhi : INC addr,X ;addr=0..$FFFF), MEM=addr+X	
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	CollapseXQ();
	arg16 += mReg.IndX;
	arg8 = mpMem->Peek8bit(arg16) + 1;
	mpMem->Poke8bit(arg16, arg8);
	SetFlags(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeInx()
 * Purpose:		Execute INX opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeInx()
{
	// INcrement X, Implied ($E8 : INX)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.isXQ) {
		qReg->INC(1, REGS_INDX_Q, REG_LEN);
	}
	mReg.IndX++;
	if (mReg.isXQ) {
		SetFlagsReg(REGS_INDX_Q);
	}
	else {
		SetFlags(mReg.IndX);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeDex()
 * Purpose:		Execute DEX opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeDex()
{
	// DEcrement X, Implied ($CA : DEX)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.isXQ) {
		qReg->DEC(1, REGS_INDX_Q, REG_LEN);
	}
	mReg.IndX--;
	if (mReg.isXQ) {
		SetFlagsReg(REGS_INDX_Q);
	}
	else {
		SetFlags(mReg.IndX);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeIny()
 * Purpose:		Execute INY opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeIny()
{
	// INcrement Y, Implied ($C8 : INY)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.isYQ) {
		qReg->INC(1, REGS_INDY_Q, REG_LEN);
	}
	mReg.IndY++;
	if (mReg.isYQ) {
		SetFlagsReg(REGS_INDY_Q);
	}
	else {
		SetFlags(mReg.IndY);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeDey()
 * Purpose:		Execute DEY opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeDey()
{
	// DEcrement Y, Implied ($88 : DEY)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.isYQ) {
		qReg->DEC(1, REGS_INDY_Q, REG_LEN);
	}
	mReg.IndY--;
	if (mReg.isYQ) {
		SetFlagsReg(REGS_INDY_Q);
	}
	else {
		SetFlags(mReg.IndY);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeJmpAbs()
 * Purpose:		Execute JMP opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeJmpAbs()
{
	// JuMP, Absolute ($4C addrlo addrhi : JMP addr ;addr=0..$FFFF),
	// MEM=addr
	mReg.PtrAddr = GetAddrWithMode(ADDRMODE_ABS);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeJmpInd()
 * Purpose:		Execute JMP opcode, IND addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeJmpInd()
{
	// JuMP, Indirect Absolute
	// ($6C addrlo addrhi : JMP (addr) ;addr=0..FFFF), MEM=&addr
	mReg.PtrAddr = GetAddrWithMode(ADDRMODE_IND);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeOraIzx()
 * Purpose:		Execute ORA opcode, IZX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeOraIzx()
{
	unsigned short arg16 = 0;
	// bitwise OR with Accumulator, Indexed Indirect
	// ($01 arg : ORA (arg,X) ;arg=0..$FF), MEM=&(arg+X)
	arg16 = GetAddrWithMode(ADDRMODE_IZX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	LogicOpAcc(arg16, LOGOP_OR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeOraZp()
 * Purpose:		Execute ORA opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeOraZp()
{
	// bitwise OR with Accumulator, Zero Page
	// ($05 arg : ORA arg ;arg=0..$FF), MEM=arg
	LogicOpAcc(GetAddrWithMode(ADDRMODE_ZP), LOGOP_OR);	
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeOraImm()
 * Purpose:		Execute ORA opcode, IMM addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeOraImm()
{
	unsigned short arg16 = 0;
	// bitwise OR with Accumulator, Immediate
	// ($09 arg : ORA #arg ;arg=0..$FF), MEM=PC+1
	arg16 = GetAddrWithMode(ADDRMODE_IMM);
	mReg.LastArg = mpMem->Peek8bit(arg16);
	LogicOpAcc(arg16, LOGOP_OR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeOraAbs()
 * Purpose:		Execute ORA opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeOraAbs()
{
	unsigned short arg16 = 0;
	// bitwise OR with Accumulator, Absolute
	// ($0D addrlo addrhi : ORA addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	LogicOpAcc(arg16, LOGOP_OR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeOraIzy()
 * Purpose:		Execute ORA opcode, IZY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeOraIzy()
{
	unsigned short arg16 = 0;
	// bitwise OR with Accumulator, Indirect Indexed
	// ($11 arg : ORA (arg),Y ;arg=0..$FF), MEM=&arg+Y
	arg16 = GetAddrWithMode(ADDRMODE_IZY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;	
	LogicOpAcc(arg16, LOGOP_OR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeOraZpx()
 * Purpose:		Execute ORA opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeOraZpx()
{
	unsigned short arg16 = 0;
	// bitwise OR with Accumulator, Zero Page Indexed, X
	// ($15 arg : ORA arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	LogicOpAcc(arg16, LOGOP_OR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeOraAby()
 * Purpose:		Execute ORA opcode, ABY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeOraAby()
{
	unsigned short arg16 = 0;
	// bitwise OR with Accumulator, Absolute Indexed, Y
	// ($19 addrlo addrhi : ORA addr,Y ;addr=0..$FFFF), MEM=addr+Y
	arg16 = GetAddrWithMode(ADDRMODE_ABY);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	LogicOpAcc(arg16, LOGOP_OR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeOraAbx()
 * Purpose:		Execute ORA opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeOraAbx()
{
	unsigned short arg16 = 0;
	// bitwise OR with Accumulator, Absolute Indexed, X
	// ($1D addrlo addrhi : ORA addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	CollapseXQ();
	arg16 += mReg.IndX;
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	LogicOpAcc(arg16, LOGOP_OR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAslZp()
 * Purpose:		Execute ASL opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAslZp()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// Arithmetic Shift Left, Zero Page ($06 arg : ASL arg ;arg=0..$FF),
	// MEM=arg	
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	arg8 = mpMem->Peek8bit(arg16);
	SetFlag(((arg8 & FLAGS_SIGN) == FLAGS_SIGN), FLAGS_CARRY);	
	SetFlags(arg8);
	arg8 = ShiftLeft(arg8);
	mpMem->Poke8bit(arg16, arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAslAcc()
 * Purpose:		Execute ASL opcode, ACC addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAslAcc()
{
	// Arithmetic Shift Left, Accumulator ($0A : ASL)
	mReg.LastAddrMode = ADDRMODE_ACC;
	mReg.Acc = ShiftLeft(mReg.Acc);
	ShiftLeftQ();
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAslAbs()
 * Purpose:		Execute ASL opcode, ACC addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAslAbs()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// Arithmetic Shift Left, Absolute
	// ($0E addrlo addrhi : ASL addr ;addr=0..$FFFF), MEM=addr	
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	arg8 = mpMem->Peek8bit(arg16);
	SetFlag(((arg8 & FLAGS_SIGN) == FLAGS_SIGN), FLAGS_CARRY);
	arg8 = ShiftLeft(arg8);
	mpMem->Poke8bit(arg16, arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAslZpx()
 * Purpose:		Execute ASL opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAslZpx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// Arithmetic Shift Left, Zero Page Indexed, X
	// ($16 arg : ASL arg,X ;arg=0..$FF), MEM=arg+X	
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	arg8 = mpMem->Peek8bit(arg16);
	SetFlag(((arg8 & FLAGS_SIGN) == FLAGS_SIGN), FLAGS_CARRY);
	arg8 = ShiftLeft(arg8);
	mpMem->Poke8bit(arg16, arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAslAbx()
 * Purpose:		Execute ASL opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAslAbx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// Arithmetic Shift Left, Absolute Indexed, X
	// ($1E addrlo addrhi : ASL addr,X ;addr=0..$FFFF), MEM=addr+X		
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	CollapseXQ();
	arg16 += mReg.IndX;
	arg8 = mpMem->Peek8bit(arg16);
	SetFlag(((arg8 & FLAGS_SIGN) == FLAGS_SIGN), FLAGS_CARRY);
	arg8 = ShiftLeft(arg8);
	mpMem->Poke8bit(arg16, arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeJsrAbs()
 * Purpose:		Execute JSR opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeJsrAbs()
{
	unsigned short arg16 = 0;
	// Jump to SubRoutine, Absolute
	// ($20 addrlo addrhi : JSR addr ;addr=0..$FFFF), MEM=addr
	// ----
	// PC - next instruction address
	// Push PC-1 on stack (HI, then LO).
	// Currently PC (mReg.PtrAddr) is at the 1-st argument of JSR.
	// Therefore the actual PC-1 used in calculations equals: 
	// mReg.PtrAddr+1
	arg16 = 0x100;
	arg16 += mReg.PtrStack--;
	// HI(PC-1) - HI part of next instr. addr. - 1
	mpMem->Poke8bit(arg16, (unsigned char) (((mReg.PtrAddr+1) & 0xFF00) >> 8));
	arg16 = 0x100;
	arg16 += mReg.PtrStack--;
	// LO(PC-1) - LO part of next instr. addr. - 1
	mpMem->Poke8bit(arg16, (unsigned char) ((mReg.PtrAddr+1) & 0x00FF));
	mReg.PtrAddr = GetAddrWithMode(ADDRMODE_ABS);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAndIzx()
 * Purpose:		Execute AND opcode, IZX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAndIzx()
{
	unsigned short arg16 = 0;
	// bitwise AND with accumulator, Indexed Indirect
	// ($21 arg : AND (arg,X) ;arg=0..$FF), MEM=&(arg+X)
	arg16 = GetAddrWithMode(ADDRMODE_IZX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	LogicOpAcc(arg16, LOGOP_AND);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAndZp()
 * Purpose:		Execute AND opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAndZp()
{
	// bitwise AND with accumulator, Zero Page
	// ($25 arg : AND arg ;arg=0..$FF), MEM=arg
	LogicOpAcc(GetAddrWithMode(ADDRMODE_ZP), LOGOP_AND);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAndImm()
 * Purpose:		Execute AND opcode, IMM addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAndImm()
{
	unsigned short arg16 = 0;
	// bitwise AND with accumulator, Immediate
	// ($29 arg : AND #arg ;arg=0..$FF), MEM=PC+1
	arg16 = GetAddrWithMode(ADDRMODE_IMM);
	mReg.LastArg = mpMem->Peek8bit(arg16);		
	LogicOpAcc(arg16, LOGOP_AND);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAndAbs()
 * Purpose:		Execute AND opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAndAbs()
{
	unsigned short arg16 = 0;
	// bitwise AND with accumulator, Absolute
	// ($2D addrlo addrhi : AND addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	LogicOpAcc(arg16, LOGOP_AND);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAndIzy()
 * Purpose:		Execute AND opcode, IZY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAndIzy()
{
	unsigned short arg16 = 0;
	// bitwise AND with accumulator, Indirect Indexed
	// ($31 arg : AND (arg),Y ;arg=0..$FF), MEM=&arg+Y
	arg16 = GetAddrWithMode(ADDRMODE_IZY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	LogicOpAcc(arg16, LOGOP_AND);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAndZpx()
 * Purpose:		Execute AND opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAndZpx()
{
	unsigned short arg16 = 0;
	// bitwise AND with accumulator, Zero Page Indexed, X
	// ($35 arg : AND arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	LogicOpAcc(arg16, LOGOP_AND);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAndAby()
 * Purpose:		Execute AND opcode, ABY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAndAby()
{
	unsigned short arg16 = 0;
	// bitwise AND with accumulator, Absolute Indexed, Y
	// ($39 addrlo addrhi : AND addr,Y ;addr=0..$FFFF), MEM=addr+Y
	arg16 = GetAddrWithMode(ADDRMODE_ABY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	LogicOpAcc(arg16, LOGOP_AND);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAndAbx()
 * Purpose:		Execute AND opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAndAbx()
{
	unsigned short arg16 = 0;
	// bitwise AND with accumulator, Absolute Indexed, X
	// ($3D addrlo addrhi : AND addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	CollapseXQ();
	arg16 += mReg.IndX;
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	LogicOpAcc(arg16, LOGOP_AND);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeBitZp()
 * Purpose:		Execute BIT opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeBitZp()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// test BITs, Zero Page ($24 arg : BIT arg ;arg=0..$FF), MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	arg8 = mpMem->Peek8bit(arg16);
	CollapseAccQ();
	SetFlag((arg8 & FLAGS_OVERFLOW) == FLAGS_OVERFLOW, FLAGS_OVERFLOW);
	SetFlag((arg8 & FLAGS_SIGN) == FLAGS_SIGN, FLAGS_SIGN);
	arg8 = (mpMem->Peek8bit(arg16)) & mReg.Acc;
	SetFlag((arg8 == 0), FLAGS_ZERO);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeBitAbs()
 * Purpose:		Execute BIT opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeBitAbs()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// test BITs, Absolute
	// ($2C addrlo addrhi : BIT addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	arg8 = mpMem->Peek8bit(arg16);
	CollapseAccQ();
	SetFlag((arg8 & FLAGS_OVERFLOW) == FLAGS_OVERFLOW, FLAGS_OVERFLOW);
	SetFlag((arg8 & FLAGS_SIGN) == FLAGS_SIGN, FLAGS_SIGN);	
	arg8 = (mpMem->Peek8bit(arg16)) & mReg.Acc;
	SetFlag((arg8 == 0), FLAGS_ZERO);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRolZp()
 * Purpose:		Execute ROL opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRolZp()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;		
	// ROtate Left, Zero Page ($26 arg : ROL arg ;arg=0..$FF), MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	arg8 = mpMem->Peek8bit(arg16);
	arg8 = RotateLeft(arg8);
	mpMem->Poke8bit(arg16, arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRolAcc()
 * Purpose:		Execute ROL opcode, ACC addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRolAcc()
{
	// ROtate Left, Accumulator ($2A : ROL)
	mReg.LastAddrMode = ADDRMODE_ACC;
	mReg.Acc = RotateLeft(mReg.Acc);
	RotateLeftQ();
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRolAbs()
 * Purpose:		Execute ROL opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRolAbs()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// ROtate Left, Absolute
	// ($2E addrlo addrhi : ROL addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	arg8 = mpMem->Peek8bit(arg16);
	arg8 = RotateLeft(arg8);
	mpMem->Poke8bit(arg16, arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRolZpx()
 * Purpose:		Execute ROL opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRolZpx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// ROtate Left, Zero Page Indexed, X
	// ($36 arg : ROL arg,X ;arg=0..$FF), MEM=arg+X		
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	arg8 = mpMem->Peek8bit(arg16);
	arg8 = RotateLeft(arg8);
	mpMem->Poke8bit(arg16, arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRolAbx()
 * Purpose:		Execute ROL opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRolAbx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// ROtate Left, Absolute Indexed, X
	// ($3E addrlo addrhi : ROL addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	CollapseXQ();
	arg16 += mReg.IndX;
	arg8 = mpMem->Peek8bit(arg16);
	arg8 = RotateLeft(arg8);
	mpMem->Poke8bit(arg16, arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodePhp()
 * Purpose:		Execute PHP opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodePhp()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;		
	// PusH Processor status on Stack, Implied ($08 : PHP)
	mReg.LastAddrMode = ADDRMODE_IMP;
	arg16 = 0x100;
	arg16 += mReg.PtrStack--;
	MeasureFlagsQ();
	arg8 = mReg.Flags | FLAGS_BRK;
	mpMem->Poke8bit(arg16, arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodePha()
 * Purpose:		Execute PHA opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodePha()
{
	unsigned short arg16 = 0;
	// PusH Accumulator, Implied ($48 : PHA)
	mReg.LastAddrMode = ADDRMODE_IMP;
	arg16 = 0x100;
	arg16 += mReg.PtrStack--;
	CollapseAccQ();
	mpMem->Poke8bit(arg16, mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodePlp()
 * Purpose:		Execute PLP opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodePlp()
{
	unsigned short arg16 = 0;
	// PuLl Processor status, Implied ($28 : PLP)
	mReg.LastAddrMode = ADDRMODE_IMP;
	arg16 = 0x100;
	arg16 += ++mReg.PtrStack;
	unsigned char flags = mpMem->Peek8bit(arg16);
	SetFlag(true, flags);
	SetFlag(false, ~flags);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodePla()
 * Purpose:		Execute PLA opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodePla()
{
	unsigned short arg16 = 0;
	// PuLl Accumulator, Implied ($68 : PLA)
	mReg.LastAddrMode = ADDRMODE_IMP;
	arg16 = 0x100;
	arg16 += ++mReg.PtrStack;
	CollapseAccQ();
	mReg.Acc = mpMem->Peek8bit(arg16);
	SetFlags(mReg.Acc);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeClq()
 * Purpose:		Clear quantum mode flag, IMPlied addressing mode.
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeClq()
{
	// CLear Oracle, Implied ($1f : CLO)
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags &= ~FLAGS_QUANTUM;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSEO()
 * Purpose:		SEt quantum mode flag, IMPlied addressing mode.
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSeq()
{
	// SEear Oracle, Implied ($3f : SEO)
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags |= FLAGS_QUANTUM;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeClc()
 * Purpose:		Execute CLC opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeClc()
{
	// CLear Carry, Implied ($18 : CLC)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.isCarryQ) CollapseCarryQ();
	mReg.Flags &= ~FLAGS_CARRY;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeHac()
 * Purpose:		Hadamard on carry flag, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeHac()
{
	// Hadamard Carry, Implied ($18 : CLC)
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareCarryQ();
	qReg->H(FLAGS_CARRY_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSec()
 * Purpose:		Execute SEC opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSec()
{
	// SEt Carry, Implied ($38 : SEC)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.isCarryQ) CollapseCarryQ();
	mReg.Flags |= FLAGS_CARRY;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCli()
 * Purpose:		Execute CLI opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCli()
{
	// CLear Interrupt, Implied ($58 : CLI)
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags &= ~FLAGS_IRQ;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeClv()
 * Purpose:		Execute CLV opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeClv()
{
	// CLear oVerflow, Implied ($B8 : CLV)
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags &= ~FLAGS_OVERFLOW;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeQzZero()
 * Purpose:		Z operator on zero flag qubit, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeQzZero()
{
	// Hadamard overflow, Implied ($18 : CLC)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.Flags & FLAGS_ZERO) qReg->PhaseFlip();
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeQzSign()
 * Purpose:		Z operator on sign flag qubit, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeQzSign()
{
	// Hadamard overflow, Implied ($18 : CLC)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.Flags & FLAGS_SIGN) qReg->PhaseFlip();
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeQzCarry()
 * Purpose:		Z operator on carry flag qubit, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeQzCarry()
{
	// Hadamard overflow, Implied ($18 : CLC)
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareCarryQ();
	qReg->Z(FLAGS_CARRY_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeQzOver()
 * Purpose:		Z operator on overflow flag qubit, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeQzOver()
{
	// Hadamard overflow, Implied ($18 : CLC)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mReg.Flags & FLAGS_OVERFLOW) qReg->PhaseFlip();
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCld()
 * Purpose:		Execute CLD opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCld()
{
	// CLear Decimal, Implied ($D8 : CLD)
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags &= ~FLAGS_DEC;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSed()
 * Purpose:		Execute SED opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSed()
{
	// SEt Decimal, Implied ($F8 : SED)
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags |= FLAGS_DEC;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSei()
 * Purpose:		Execute SEI opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSei()
{
	// SEt Interrupt, Implied ($78 : SEI)
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags |= FLAGS_IRQ;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRti()
 * Purpose:		Execute RTI opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRti()
{
	unsigned short arg16 = 0;
	/* 
	* RTI retrieves the Processor Status Word (flags) and the Program
	* Counter from the stack in that order
	* (interrupts push the PC first and then the PSW). 
	* Note that unlike RTS, the return address on the stack is the
	* actual address rather than the address-1. 
	*/			
	// ReTurn from Interrupt, Implied ($40 : RTI)
	mReg.LastAddrMode = ADDRMODE_IMP;
	arg16 = 0x100;
	arg16 += ++mReg.PtrStack;

	mReg.Flags &= ~FLAGS_QUANTUM;
	unsigned char flags = mpMem->Peek8bit(arg16);
	SetFlag(true, flags);
	SetFlag(false, ~flags);

	arg16++; mReg.PtrStack++;
	mReg.PtrAddr = mpMem->Peek8bit(arg16);
	arg16++; mReg.PtrStack++;
	mReg.PtrAddr += (mpMem->Peek8bit(arg16) * 0x100);
	mReg.SoftIrq = CheckFlag(FLAGS_BRK);
	SetFlag(false,FLAGS_IRQ);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRts()
 * Purpose:		Execute RTS opcode, IMPlied addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRts()
{
	unsigned short arg16 = 0;
	// ReTurn from Subroutine, Implied ($60 : RTS)
	mReg.LastAddrMode = ADDRMODE_IMP;
	if (mExitAtLastRTS && mReg.PtrStack == 0xFF) {
		mReg.LastRTS = true;
	} else {
		arg16 = 0x100;
		arg16 += ++mReg.PtrStack;
		mReg.PtrAddr = mpMem->Peek8bit(arg16);
		arg16++; mReg.PtrStack++;
		mReg.PtrAddr += (mpMem->Peek8bit(arg16) * 0x100);
		mReg.PtrAddr++;
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeEorIzx()
 * Purpose:		Execute EOR opcode, IZX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeEorIzx()
{
	unsigned short arg16 = 0;
	// bitwise Exclusive OR, Indexed Indirect
	// ($41 arg : EOR (arg,X) ;arg=0..$FF), MEM=&(arg+X)
	arg16 = GetAddrWithMode(ADDRMODE_IZX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	LogicOpAcc(arg16, LOGOP_EOR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeEorZp()
 * Purpose:		Execute EOR opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeEorZp()
{
	// bitwise Exclusive OR, Zero Page ($45 arg : EOR arg ;arg=0..$FF),
	// MEM=arg
	LogicOpAcc(GetAddrWithMode(ADDRMODE_ZP), LOGOP_EOR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeEorImm()
 * Purpose:		Execute EOR opcode, IMM addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeEorImm()
{
	unsigned short arg16 = 0;
	// bitwise Exclusive OR, Immediate ($49 arg : EOR #arg ;arg=0..$FF),
	// MEM=PC+1
	arg16 = GetAddrWithMode(ADDRMODE_IMM);
	mReg.LastArg = mpMem->Peek8bit(arg16);		
	LogicOpAcc(arg16, LOGOP_EOR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeEorAbs()
 * Purpose:		Execute EOR opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeEorAbs()
{
	unsigned short arg16 = 0;
	// bitwise Exclusive OR, Absolute
	// ($4D addrlo addrhi : EOR addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	LogicOpAcc(arg16, LOGOP_EOR);		
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeEorIzy()
 * Purpose:		Execute EOR opcode, IZY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeEorIzy()
{
	unsigned short arg16 = 0;
	// bitwise Exclusive OR, Indirect Indexed
	// ($51 arg : EOR (arg),Y ;arg=0..$FF), MEM=&arg+Y
	arg16 = GetAddrWithMode(ADDRMODE_IZY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	LogicOpAcc(arg16, LOGOP_EOR);		
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeEorZpx()
 * Purpose:		Execute EOR opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeEorZpx()
{
	unsigned short arg16 = 0;
	// bitwise Exclusive OR, Zero Page Indexed, X
	// ($55 arg : EOR arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	LogicOpAcc(arg16, LOGOP_EOR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeEorAby()
 * Purpose:		Execute EOR opcode, ABY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeEorAby()
{
	unsigned short arg16 = 0;
	// bitwise Exclusive OR, Absolute Indexed, Y
	// ($59 addrlo addrhi : EOR addr,Y ;addr=0..$FFFF), MEM=addr+Y
	arg16 = GetAddrWithMode(ADDRMODE_ABY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	LogicOpAcc(arg16, LOGOP_EOR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeEorAbx()
 * Purpose:		Execute EOR opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeEorAbx()
{
	unsigned short arg16 = 0;
	// bitwise Exclusive OR, Absolute Indexed, X
	// ($5D addrlo addrhi : EOR addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	CollapseXQ();
	arg16 += mReg.IndX;
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	LogicOpAcc(arg16, LOGOP_EOR);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLsrZp()
 * Purpose:		Execute LSR opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLsrZp()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// Logical Shift Right, Zero Page ($46 arg : LSR arg ;arg=0..$FF),
	// MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	arg8 = mpMem->Peek8bit(arg16);
	SetFlag(((arg8 & 0x01) == 0x01), FLAGS_CARRY);
	arg8 = ShiftRight(arg8);
	mpMem->Poke8bit(arg16, arg8);		
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLsrAcc()
 * Purpose:		Execute LSR opcode, ACC addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLsrAcc()
{
	// Logical Shift Right, Accumulator ($4A : LSR)
	mReg.LastAddrMode = ADDRMODE_ACC;
	mReg.Acc = ShiftRight(mReg.Acc);
	ShiftRightQ();
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLsrAbs()
 * Purpose:		Execute LSR opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLsrAbs()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// Logical Shift Right, Absolute
	// ($4E addrlo addrhi : LSR addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	arg8 = mpMem->Peek8bit(arg16);
	SetFlag(((arg8 & 0x01) == 0x01), FLAGS_CARRY);
	arg8 = ShiftRight(arg8);
	mpMem->Poke8bit(arg16, arg8);		
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLsrZpx()
 * Purpose:		Execute LSR opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLsrZpx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;		
	// Logical Shift Right, Zero Page Indexed, X
	// ($56 arg : LSR arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	arg8 = mpMem->Peek8bit(arg16);
	SetFlag(((arg8 & 0x01) == 0x01), FLAGS_CARRY);
	arg8 = ShiftRight(arg8);
	mpMem->Poke8bit(arg16, arg8);		
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeLsrAbx()
 * Purpose:		Execute LSR opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeLsrAbx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// Logical Shift Right, Absolute Indexed, X
	// ($5E addrlo addrhi : LSR addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	CollapseXQ();
	arg16 += mReg.IndX;
	arg8 = mpMem->Peek8bit(arg16);
	SetFlag(((arg8 & 0x01) == 0x01), FLAGS_CARRY);
	arg8 = ShiftRight(arg8);
	mpMem->Poke8bit(arg16, arg8);		
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAdcIzx()
 * Purpose:		Execute ADC opcode, IZX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAdcIzx()
{
	unsigned short arg16 = 0;
	// ADd with Carry, Indexed Indirect
	// ($61 arg : ADC (arg,X) ;arg=0..$FF), MEM=&(arg+X)
	arg16 = GetAddrWithMode(ADDRMODE_IZX);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isXQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit((arg16 + i) & 0xFF);
		}
		PrepareAccQ();
		PrepareCarryQ();
		mReg.Acc = qReg->IndexedADC(REG_LEN, REG_LEN, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q, toLoad);
		if (qReg->Prob(FLAGS_CARRY_Q) >= 0.5) {
			mReg.Flags |= FLAGS_CARRY;
		}
		else {
			mReg.Flags &= ~FLAGS_CARRY;
		}
	}
	else {
		arg16 = (arg16 + mReg.IndX) & 0xFF;
		unsigned char toAdd = mpMem->Peek8bit(arg16);
		AddWithCarry(toAdd);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAdcZp()
 * Purpose:		Execute ADC opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAdcZp()
{
	unsigned short arg16 = 0;
	// ADd with Carry, Zero Page ($65 arg : ADC arg ;arg=0..$FF),
	// MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	unsigned char toAdd = mpMem->Peek8bit(arg16);
	AddWithCarry(toAdd);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAdcImm()
 * Purpose:		Execute ADC opcode, IMM addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAdcImm()
{
	// ADd with Carry, Immediate ($69 arg : ADC #arg ;arg=0..$FF),
	// MEM=PC+1
	mReg.LastArg = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_IMM));
	AddWithCarry(mReg.LastArg);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAdcAbs()
 * Purpose:		Execute ADC opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAdcAbs()
{
	unsigned short arg16 = 0;
	// ADd with Carry, Absolute
	// ($6D addrlo addrhi : ADC addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	unsigned char toAdd = mpMem->Peek8bit(arg16);
	AddWithCarry(toAdd);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAdcIzy()
 * Purpose:		Execute ADC opcode, IZY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAdcIzy()
{
	unsigned short arg16 = 0;
	// ADd with Carry, Indirect Indexed
	// ($71 arg : ADC (arg),Y ;arg=0..$FF), MEM=&arg+Y
	arg16 = GetAddrWithMode(ADDRMODE_IZY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	unsigned char toAdd = mpMem->Peek8bit(arg16);
	AddWithCarry(toAdd);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAdcZpx()
 * Purpose:		Execute ADC opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAdcZpx()
{
	unsigned short arg16 = 0;
	// ADd with Carry, Zero Page Indexed, X
	// ($75 arg : ADC arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isXQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit((arg16 + i) & 0xFF);
		}
		PrepareAccQ();
		PrepareCarryQ();
		mReg.Acc = qReg->IndexedADC(REG_LEN, REG_LEN, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q, toLoad);
		if (qReg->Prob(FLAGS_CARRY_Q) >= 0.5) {
			mReg.Flags |= FLAGS_CARRY;
		}
		else {
			mReg.Flags &= ~FLAGS_CARRY;
		}
	}
	else {
		arg16 = (arg16 + mReg.IndX) & 0xFF;
		unsigned char toAdd = mpMem->Peek8bit(arg16);
		AddWithCarry(toAdd);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAdcAby()
 * Purpose:		Execute ADC opcode, ABY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAdcAby()
{
	unsigned short arg16 = 0;
	// ADd with Carry, Absolute Indexed, Y
	// ($79 addrlo addrhi : ADC addr,Y ;addr=0..$FFFF), MEM=addr+Y
	arg16 = GetAddrWithMode(ADDRMODE_ABY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	unsigned char toAdd = mpMem->Peek8bit(arg16);
	AddWithCarry(toAdd);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeAdcAbx()
 * Purpose:		Execute ADC opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeAdcAbx()
{
	unsigned short arg16 = 0;
	// ADd with Carry, Absolute Indexed, X
	// ($7D addrlo addrhi : ADC addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isXQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit(arg16 + i);
		}
		PrepareAccQ();
		PrepareCarryQ();
		mReg.Acc = qReg->IndexedADC(REGS_INDX_Q, REG_LEN, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q, toLoad);
		if (qReg->Prob(FLAGS_CARRY_Q) >= 0.5) {
			mReg.Flags |= FLAGS_CARRY;
		}
		else {
			mReg.Flags &= ~FLAGS_CARRY;
		}
		SetFlags(REGS_ACC_Q);
	}
	else {
		arg16 += mReg.IndX;
		unsigned char toAdd = mpMem->Peek8bit(arg16);
		AddWithCarry(toAdd);
	}
	if (mReg.PageBoundary) mReg.CyclesLeft++;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRorZp()
 * Purpose:		Execute ROR opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRorZp()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// ROtate Right, Zero Page ($66 arg : ROR arg ;arg=0..$FF), MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	arg8 = mpMem->Peek8bit(arg16);
	mpMem->Poke8bit(arg16, RotateRight(arg8));		
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRorAcc()
 * Purpose:		Execute ROR opcode, ACC addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRorAcc()
{
	// ROtate Right, Accumulator ($6A : ROR)
	mReg.LastAddrMode = ADDRMODE_ACC;
	mReg.Acc = RotateRight(mReg.Acc);
	RotateRightQ();
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRorAbs()
 * Purpose:		Execute ROR opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRorAbs()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// ROtate Right, Absolute
	// ($6E addrlo addrhi : ROR addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	arg8 = mpMem->Peek8bit(arg16);
	mpMem->Poke8bit(arg16, RotateRight(arg8));		
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRorZpx()
 * Purpose:		Execute ROR opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRorZpx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// ROtate Right, Zero Page Indexed, X
	// ($76 arg : ROR arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	arg8 = mpMem->Peek8bit(arg16);
	mpMem->Poke8bit(arg16, RotateRight(arg8));		
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRorAbx()
 * Purpose:		Execute ROR opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRorAbx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// ROtate Right, Absolute Indexed, X
	// ($7E addrlo addrhi : ROR addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	CollapseXQ();
	arg16 += mReg.IndX;
	arg8 = mpMem->Peek8bit(arg16);
	mpMem->Poke8bit(arg16, RotateRight(arg8));
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCpyImm()
 * Purpose:		Execute CPY opcode, IMM addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCpyImm()
{
	unsigned char arg8 = 0;
	// ComPare Y register, Immediate ($C0 arg : CPY #arg ;arg=0..$FF),
	// MEM=PC+1
	mReg.LastArg = arg8 = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_IMM));
	CompareOpIndY(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCpyZp()
 * Purpose:		Execute CPY opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCpyZp()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// ComPare Y register, Zero Page ($C4 arg : CPY arg ;arg=0..$FF), MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	arg8 = mpMem->Peek8bit(arg16);
	CompareOpIndY(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCpyAbs()
 * Purpose:		Execute CPY opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCpyAbs()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;		
	// ComPare Y register, Absolute
	// ($CC addrlo addrhi : CPY addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	arg8 = mpMem->Peek8bit(arg16);
	CompareOpIndY(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCmpIzx()
 * Purpose:		Execute CMP opcode, IZX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCmpIzx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// CoMPare accumulator, Indexed Indirect
	// ($A1 arg : LDA (arg,X) ;arg=0..$FF), MEM=&(arg+X)
	arg16 = GetAddrWithMode(ADDRMODE_IZX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	arg8 = mpMem->Peek8bit(arg16);
	CompareOpAcc(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCmpZp()
 * Purpose:		Execute CMP opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCmpZp()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// CoMPare accumulator, Zero Page ($C5 arg : CMP arg ;arg=0..$FF),
	// MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	arg8 = mpMem->Peek8bit(arg16);
	CompareOpAcc(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCmpImm()
 * Purpose:		Execute CMP opcode, IMM addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCmpImm()
{
	unsigned char arg8 = 0;
	// CoMPare accumulator, Immediate ($C9 arg : CMP #arg ;arg=0..$FF),
	// MEM=PC+1
	arg8 = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_IMM));
	CompareOpAcc(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCmpAbs()
 * Purpose:		Execute CMP opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCmpAbs()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// CoMPare accumulator, Absolute
	// ($CD addrlo addrhi : CMP addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	arg8 = mpMem->Peek8bit(arg16);
	CompareOpAcc(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCmpIzy()
 * Purpose:		Execute CMP opcode, IZY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCmpIzy()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// CoMPare accumulator, Indirect Indexed
	// ($D1 arg : CMP (arg),Y ;arg=0..$FF), MEM=&arg+Y
	arg16 = GetAddrWithMode(ADDRMODE_IZY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	arg8 = mpMem->Peek8bit(arg16);
	CompareOpAcc(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCmpZpx()
 * Purpose:		Execute CMP opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCmpZpx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// CoMPare accumulator, Zero Page Indexed, X
	// ($D5 arg : CMP arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	arg8 = mpMem->Peek8bit(arg16);
	CompareOpAcc(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCmpAby()
 * Purpose:		Execute CMP opcode, ABY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCmpAby()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// CoMPare accumulator, Absolute Indexed, Y
	// ($D9 addrlo addrhi : CMP addr,Y ;addr=0..$FFFF), MEM=addr+Y
	arg16 = GetAddrWithMode(ADDRMODE_ABY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	arg8 = mpMem->Peek8bit(arg16);
	CompareOpAcc(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCmpAbx()
 * Purpose:		Execute CMP opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCmpAbx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// CoMPare accumulator, Absolute Indexed, X
	// ($DD addrlo addrhi : CMP addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	CollapseXQ();
	arg16 += mReg.IndX;
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	arg8 = mpMem->Peek8bit(arg16);
	CompareOpAcc(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeDecZp()
 * Purpose:		Execute DEC opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeDecZp()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// DECrement memory, Zero Page
	// ($C6 arg : DEC arg ;arg=0..$FF), MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	arg8 = mpMem->Peek8bit(arg16) - 1;
	mpMem->Poke8bit(arg16, arg8);
	SetFlags(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeDecAbs()
 * Purpose:		Execute DEC opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeDecAbs()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// DECrement memory, Absolute
	// ($CE addrlo addrhi : CMP addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	arg8 = mpMem->Peek8bit(arg16) - 1;
	mpMem->Poke8bit(arg16, arg8);
	SetFlags(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeDecZpx()
 * Purpose:		Execute DEC opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeDecZpx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// DECrement memory, Zero Page Indexed, X
	// ($D6 arg : DEC arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	CollapseXQ();
	arg16 = (arg16 + mReg.IndX) & 0xFF;
	arg8 = mpMem->Peek8bit(arg16) - 1;
	mpMem->Poke8bit(arg16, arg8);
	SetFlags(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeDecAbx()
 * Purpose:		Execute DEC opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeDecAbx()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// DECrement memory, Absolute Indexed, X
	// ($DE addrlo addrhi : DEC addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	CollapseXQ();
	arg16 += mReg.IndX;
	arg8 = mpMem->Peek8bit(arg16) - 1;
	mpMem->Poke8bit(arg16, arg8);
	SetFlags(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCpxImm()
 * Purpose:		Execute CPX opcode, IMM addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCpxImm()
{
	unsigned char arg8 = 0;
	// ComPare X register, Immediate ($E0 arg : CPX #arg ;arg=0..$FF),
	// MEM=PC+1
	mReg.LastArg = arg8 = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_IMM));
	CompareOpIndX(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCpxZp()
 * Purpose:		Execute CPX opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCpxZp()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;	
	// ComPare X register, Zero Page ($E4 arg : CPX arg ;arg=0..$FF),
	// MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	arg8 = mpMem->Peek8bit(arg16);
	CompareOpIndX(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCpxAbs()
 * Purpose:		Execute CPX opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCpxAbs()
{
	unsigned char arg8 = 0;
	unsigned short arg16 = 0;
	// ComPare X register, Absolute
	// ($EC addrlo addrhi : CPX addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	arg8 = mpMem->Peek8bit(arg16);
	CompareOpIndX(arg8);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSbcZp()
 * Purpose:		Execute SBC opcode, ZP addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSbcZp()
{
	unsigned short arg16 = 0;
	// SuBtract with Carry, Zero Page ($E5 arg : SBC arg ;arg=0..$FF),
	// MEM=arg
	arg16 = GetAddrWithMode(ADDRMODE_ZP);
	unsigned char toSub = mpMem->Peek8bit(arg16);
	SubWithCarry(toSub);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSbcAbs()
 * Purpose:		Execute SBC opcode, ABS addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSbcAbs()
{
	unsigned short arg16 = 0;
	// SuBtract with Carry, Absolute
	// ($ED addrlo addrhi : SBC addr ;addr=0..$FFFF), MEM=addr
	arg16 = GetAddrWithMode(ADDRMODE_ABS);
	unsigned char toSub = mpMem->Peek8bit(arg16);
	SubWithCarry(toSub);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSbcIzx()
 * Purpose:		Execute SBC opcode, IZX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSbcIzx()
{
	unsigned short arg16 = 0;
	// SuBtract with Carry, Indexed Indirect
	// ($E1 arg : SBC (arg,X) ;arg=0..$FF), MEM=&(arg+X)
	arg16 = GetAddrWithMode(ADDRMODE_IZX);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isXQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit((arg16 + i) & 0xFF);
		}
		PrepareAccQ();
		PrepareCarryQ();
		mReg.Acc = qReg->IndexedSBC(REG_LEN, REG_LEN, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q, toLoad);
		if (qReg->Prob(FLAGS_CARRY_Q) >= 0.5) {
			mReg.Flags |= FLAGS_CARRY;
		}
		else {
			mReg.Flags &= ~FLAGS_CARRY;
		}
	}
	else {
		arg16 = (arg16 + mReg.IndX) & 0xFF;
		unsigned char toSub = mpMem->Peek8bit(arg16);
		SubWithCarry(toSub);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSbcIzy()
 * Purpose:		Execute SBC opcode, IZY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSbcIzy()
{
	unsigned short arg16 = 0;
	// SuBtract with Carry, Indirect Indexed
	// ($F1 arg : SBC (arg),Y ;arg=0..$FF), MEM=&arg+Y
	arg16 = GetAddrWithMode(ADDRMODE_IZY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	unsigned char toSub = mpMem->Peek8bit(arg16);
	SubWithCarry(toSub);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSbcZpx()
 * Purpose:		Execute SBC opcode, ZPX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSbcZpx()
{
	unsigned short arg16 = 0;
	// SuBtract with Carry, Zero Page Indexed, X
	// ($F5 arg : SBC arg,X ;arg=0..$FF), MEM=arg+X
	arg16 = GetAddrWithMode(ADDRMODE_ZPX);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isXQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit((arg16 + i) & 0xFF);
		}
		PrepareAccQ();
		PrepareCarryQ();
		mReg.Acc = qReg->IndexedSBC(REG_LEN, REG_LEN, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q, toLoad);
		if (qReg->Prob(FLAGS_CARRY_Q) >= 0.5) {
			mReg.Flags |= FLAGS_CARRY;
		}
		else {
			mReg.Flags &= ~FLAGS_CARRY;
		}
	}
	else {
		arg16 = (arg16 + mReg.IndX) & 0xFF;
		unsigned char toSub = mpMem->Peek8bit(arg16);
		SubWithCarry(toSub);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSbcAby()
 * Purpose:		Execute SBC opcode, ABY addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSbcAby()
{
	unsigned short arg16 = 0;
	// SuBtract with Carry, Absolute Indexed, Y
	// ($F9 addrlo addrhi : SBC addr,Y ;addr=0..$FFFF), MEM=addr+Y
	arg16 = GetAddrWithMode(ADDRMODE_ABY);
	if (mReg.PageBoundary) mReg.CyclesLeft++;
	unsigned char toSub = mpMem->Peek8bit(arg16);
	SubWithCarry(toSub);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSbcAbx()
 * Purpose:		Execute SBC opcode, ABX addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSbcAbx()
{
	unsigned short arg16 = 0;
	// SuBtract with Carry, Absolute Indexed, X
	// ($FD addrlo addrhi : SBC addr,X ;addr=0..$FFFF), MEM=addr+X
	arg16 = GetAddrWithMode(ADDRMODE_ABX);
	if (CheckFlag(FLAGS_QUANTUM) && mReg.isXQ) {
		unsigned char toLoad[256];
		for (int i = 0; i < 256; i++) {
			toLoad[i] = mpMem->Peek8bit(arg16 + i);
		}
		PrepareAccQ();
		PrepareCarryQ();
		mReg.Acc = qReg->IndexedSBC(REGS_INDX_Q, REG_LEN, REGS_ACC_Q, REG_LEN, FLAGS_CARRY_Q, toLoad);
		if (qReg->Prob(FLAGS_CARRY_Q) >= 0.5) {
			mReg.Flags |= FLAGS_CARRY;
		}
		else {
			mReg.Flags &= ~FLAGS_CARRY;
		}
		SetFlagsReg(REGS_ACC_Q);
	}
	else {
		CollapseXQ();
		arg16 += mReg.IndX;
		unsigned char toSub = mpMem->Peek8bit(arg16);
		SubWithCarry(toSub);
	}
	if (mReg.PageBoundary) mReg.CyclesLeft++;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSbcImm()
 * Purpose:		Execute SBC opcode, IMM addressing mode.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSbcImm()
{
	// SuBtract with Carry, Immediate ($E9 arg : SBC #arg ;arg=0..$FF),
	// MEM=PC+1
	mReg.LastArg = mpMem->Peek8bit(GetAddrWithMode(ADDRMODE_IMM));
	SubWithCarry(mReg.LastArg);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeDud()
 * Purpose:		Dummy opcode method - do nothing.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeDud()
{
	// this method body left intentionally empty
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeHadAcc()
 * Purpose:		Apply Hadamard to each bit in Acc and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeHadA()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareAccQ();
	qReg->H(REGS_ACC_Q, REG_LEN);
	mReg.Acc = RotateClassical(mReg.Acc);
	SetFlagsReg(REGS_ACC_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeHadX()
 * Purpose:		Apply Hadamard to each bit in IndX and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeHadX()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareXQ();
	qReg->H(REGS_INDX_Q, REG_LEN);
	mReg.IndX = RotateClassical(mReg.IndX);
	SetFlagsReg(REGS_INDX_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeHadY()
 * Purpose:		Apply Hadamard to each bit in IndY and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeHadY()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareYQ();
	qReg->H(REGS_INDY_Q, REG_LEN);
	mReg.IndY = RotateClassical(mReg.IndY);
	SetFlagsReg(REGS_INDY_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeXA()
 * Purpose:		Apply Pauli X to each bit in Acc and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeXA()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Acc = ~mReg.Acc;
	if (mReg.isAccQ) {
		qReg->X(REGS_ACC_Q, REG_LEN);
		SetFlagsReg(REGS_ACC_Q);
	}
	else {
		SetFlags(mReg.Acc);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeXX()
 * Purpose:		Apply Pauli X to each bit in IndX and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeXX()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.IndX = ~mReg.IndX;
	if (mReg.isXQ) {
		qReg->X(REGS_INDX_Q, REG_LEN);
		SetFlagsReg(REGS_INDX_Q);
	}
	else {
		SetFlags(mReg.IndX);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeXY()
 * Purpose:		Apply Pauli X to each bit in IndY and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeXY()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.IndY = ~mReg.IndY;
	if (mReg.isYQ) {
		qReg->X(REGS_INDY_Q, REG_LEN);
		SetFlagsReg(REGS_INDY_Q);
	}
	else {
		SetFlags(mReg.IndY);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeXC()
 * Purpose:		Apply Pauli X to Carry flag
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeXC()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags ^= FLAGS_CARRY;
	if (mReg.isCarryQ) {
		qReg->X(FLAGS_CARRY_Q);
	}
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeYA()
 * Purpose:		Apply Pauli Y to each bit in Acc and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeYA()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareAccQ();
	qReg->Y(REGS_ACC_Q, REG_LEN);
	SetFlagsReg(REGS_ACC_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeYX()
 * Purpose:		Apply Pauli Y to each bit in IndX and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeYX()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareXQ();
	qReg->Y(REGS_INDX_Q, REG_LEN);
	SetFlagsReg(REGS_INDX_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeYY()
 * Purpose:		Apply Pauli Y to each bit in IndY and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeYY()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareYQ();
	qReg->Y(REGS_INDY_Q, REG_LEN);
	SetFlagsReg(REGS_INDY_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeZA()
 * Purpose:		Apply Pauli Z to each bit in Acc and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeZA()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareAccQ();
	qReg->Z(REGS_ACC_Q, REG_LEN);
	SetFlagsReg(REGS_ACC_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeZX()
 * Purpose:		Apply Pauli Z to each bit in IndX and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeZX()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareXQ();
	qReg->Z(REGS_INDX_Q, REG_LEN);
	SetFlagsReg(REGS_INDX_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeZY()
 * Purpose:		Apply Pauli Z to each bit in IndY and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeZY()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareYQ();
	qReg->Z(REGS_INDY_Q, REG_LEN);
	SetFlagsReg(REGS_INDY_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRTA()
 * Purpose:		Apply quarter rotation around |1> axis to each bit in Acc and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRTA()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareAccQ();
	qReg->RT(M_PI / 2.0, REGS_ACC_Q, REG_LEN);
	SetFlagsReg(REGS_ACC_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRTX()
 * Purpose:		Apply quarter rotation around |1> axis to each bit in IndX and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRTX()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareXQ();
	qReg->RT(M_PI / 2.0, REGS_INDX_Q, REG_LEN);
	SetFlagsReg(REGS_INDX_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRTY()
 * Purpose:		Apply quarter rotation around |1> axis to each bit in IndY and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRTY()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareYQ();
	qReg->RT(M_PI / 2.0, REGS_INDY_Q, REG_LEN);
	SetFlagsReg(REGS_INDY_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRXA()
 * Purpose:		Apply quarter rotation around X axis to each bit in Acc and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRXA()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareAccQ();
	qReg->RX(M_PI / 2.0, REGS_ACC_Q, REG_LEN);
	mReg.Acc = RotateClassical(mReg.Acc);
	SetFlagsReg(REGS_ACC_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRXX()
 * Purpose:		Apply quarter rotation around X axis to each bit in IndX and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRXX()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareXQ();
	qReg->RX(M_PI / 2.0, REGS_INDX_Q, REG_LEN);
	mReg.IndX = RotateClassical(mReg.IndX);
	SetFlagsReg(REGS_INDX_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRXY()
 * Purpose:		Apply quarter rotation around Y axis to each bit in IndX and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRXY()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareYQ();
	qReg->RX(M_PI / 2.0, REGS_INDY_Q, REG_LEN);
	mReg.IndY = RotateClassical(mReg.IndY);
	SetFlagsReg(REGS_INDY_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRYA()
 * Purpose:		Apply quarter rotation around Y axis to each bit in Acc and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRYA()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareAccQ();
	qReg->RY(M_PI / 2.0, REGS_ACC_Q, REG_LEN);
	mReg.Acc = RotateClassical(mReg.Acc);
	SetFlagsReg(REGS_ACC_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRYX()
 * Purpose:		Apply quarter rotation around Y axis to each bit in IndX and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRYX()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareXQ();
	qReg->RY(M_PI / 2.0, REGS_INDX_Q, REG_LEN);
	mReg.IndX = RotateClassical(mReg.IndX);
	SetFlagsReg(REGS_INDX_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRYY()
 * Purpose:		Apply quarter rotation around Y axis to each bit in IndY and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRYY()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareYQ();
	qReg->RY(M_PI / 2.0, REGS_INDY_Q, REG_LEN);
	mReg.IndY = RotateClassical(mReg.IndY);
	SetFlagsReg(REGS_INDY_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRZA()
 * Purpose:		Apply quarter rotation around Z axis to each bit in Acc and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRZA()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareAccQ();
	qReg->RZ(M_PI / 2.0, REGS_ACC_Q, REG_LEN);
	mReg.Acc = RotateClassical(mReg.Acc);
	SetFlagsReg(REGS_ACC_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRZX()
 * Purpose:		Apply quarter rotation around Z axis to each bit in IndX and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRZX()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareXQ();
	qReg->RZ(M_PI / 2.0, REGS_INDX_Q, REG_LEN);
	mReg.IndX = RotateClassical(mReg.IndX);
	SetFlagsReg(REGS_INDX_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeRZY()
 * Purpose:		Apply quarter rotation around Z axis to each bit in IndY and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeRZY()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareYQ();
	qReg->RZ(M_PI / 2.0, REGS_INDY_Q, REG_LEN);
	mReg.IndY = RotateClassical(mReg.IndY);
	SetFlagsReg(REGS_INDY_Q);
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeFTA()
 * Purpose:		Fourier transform Acc and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeFTA()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareAccQ();
	qReg->QFT(REGS_ACC_Q, REG_LEN);
	SetFlagsReg(REGS_ACC_Q);
	//TODO: Implement classical Fourier transform, here.
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeFTX()
 * Purpose:		Fourier transform IndX and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeFTX()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareXQ();
	qReg->QFT(REGS_INDX_Q, REG_LEN);
	SetFlagsReg(REGS_INDX_Q);
	//TODO: Implement classical Fourier transform, here.
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeFTY()
 * Purpose:		Fourier transform IndY and set flags
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeFTY()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	PrepareYQ();
	qReg->QFT(REGS_INDY_Q, REG_LEN);
	SetFlagsReg(REGS_INDY_Q);
	//TODO: Implement classical Fourier transform, here.
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSen()
 * Purpose:		SEt Negative flag, for quantum mode
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSen()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags |= FLAGS_SIGN;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeCln()
 * Purpose:		CLear Negative flag, for quantum mode
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeCln()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags &= ~FLAGS_SIGN;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSev()
 * Purpose:		SEt oVerflow flag, for quantum mode
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSev()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags |= FLAGS_OVERFLOW;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeSez()
 * Purpose:		SEt Zero flag, for quantum mode
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeSez()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags |= FLAGS_ZERO;
}

/*
 *--------------------------------------------------------------------
 * Method:		OpCodeClz()
 * Purpose:		CLear Zero flag, for quantum mode
 * Arguments:		n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::OpCodeClz()
{
	mReg.LastAddrMode = ADDRMODE_IMP;
	mReg.Flags &= ~FLAGS_ZERO;
}

/*
 *--------------------------------------------------------------------
 * Method:		ExecOpcode()
 * Purpose:		Execute VM's opcode.
 * Arguments:	memaddr - address of code in virtual memory.
 * Returns:		Pointer to CPU registers and flags structure.
 * NOTE:
 *   Single call to this routine is considered a single CPU cycle.
 *   All opcodes take more than one cycle to complete, so this
 *   method employs counting the cycles, which are decremented
 *   in mReg.CyclesLeft counter. When mReg.CyclesLeft reaches 0
 *   the opcode execution can be condidered completed.
 *--------------------------------------------------------------------
 */
Regs *MKCpu::ExecOpcode(unsigned short memaddr)
{
	mReg.PtrAddr = memaddr;
	mReg.LastAddr = memaddr;
	unsigned char opcode = OPCODE_BRK;

	// The op-code action was executed already once.
	// Now skip if the clock cycles for this op-code are not yet completed.
	if (mReg.CyclesLeft > 0) {
		mReg.CyclesLeft--;
		return &mReg;
	}

	// If no IRQ waiting, get the next opcode and advance PC.
	// Otherwise the opcode is OPCODE_BRK and with IrqPending
	// flag set the IRQ sequence will be executed.
	if (!mReg.IrqPending) {
		opcode = mpMem->Peek8bit(mReg.PtrAddr++);
	}

	// load CPU instruction details from map
	OpCode *instrdet = &mOpCodesMap[(eOpCodes)opcode];
	
	SetFlag(false, FLAGS_BRK);	// reset BRK flag - we want to detect
	mReg.SoftIrq = false;				// software interrupt each time it
															// happens (non-maskable)
	mReg.LastRTS = false;
	mReg.LastOpCode = opcode;
	mReg.LastAddrMode = ADDRMODE_UND;
	mReg.LastArg = 0;

	string s = (instrdet->amf.length() > 0 
							? instrdet->amf.c_str() : "???");

	if (s.compare("ILL") == 0) {
		// trap any illegal opcode
		mReg.SoftIrq = true;	
	} else {
		// reset remaining cycles counter and execute legal opcode
		mReg.CyclesLeft = instrdet->time - 1;
		OpCodeHdlrFn pfun = instrdet->pfun;
		if (NULL != pfun) (this->*pfun)();
	}
				
	// Update history/log of recently executed op-codes/instructions.
	if (mEnableHistory) {

		OpCodeHistItem histentry;
		histentry.LastAddr = mReg.LastAddr;
		histentry.Acc = mReg.Acc;
		histentry.IndX = mReg.IndX;
		histentry.IndY = mReg.IndY;
		histentry.Flags = mReg.Flags;
		histentry.PtrStack = mReg.PtrStack;
		histentry.LastOpCode = mReg.LastOpCode;
		histentry.LastAddrMode = mReg.LastAddrMode;
		histentry.LastArg = mReg.LastArg;
		Add2History(histentry);
	}
	
	return &mReg;
}

/*
 *--------------------------------------------------------------------
 * Method:		GetRegs()
 * Purpose:		Return pointer to CPU registers and status.
 * Arguments:	n/a
 * Returns:		pointer to Regs structure.
 *--------------------------------------------------------------------
 */
Regs *MKCpu::GetRegs()
{
	return &mReg;
}

/*
 *--------------------------------------------------------------------
 * Method:		Add2History()
 * Purpose:		Add entry with last executed op-code, arguments and
 *            CPU status to execute history.
 * Arguments:	s - string (entry)
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::Add2History(OpCodeHistItem histitem)
{
	mExecHistory.push(histitem);
	while (mExecHistory.size() > OPCO_HIS_SIZE) mExecHistory.pop();
}

/*
 *--------------------------------------------------------------------
 * Method:		GetExecHistory()
 * Purpose:		Disassemble op-codes execute history stored in
 *            mExecHistory and create/return queue of strings with
 *            execute history in symbolic form (assembly mnemonics,
 *            properly converted arguments in corresponding addressing 
 *            mode notation that adheres to MOS-6502 industry
 *            standard.)
 * Arguments:	n/a
 * Returns:		queue<string>
 *--------------------------------------------------------------------
 */
queue<string>	MKCpu::GetExecHistory()
{
	queue<string> ret;
	queue<OpCodeHistItem> exechist(mExecHistory);

	while (exechist.size()) {
		OpCodeHistItem item = exechist.front();
		Disassemble(&item);
		char histentry[80];
		sprintf(histentry, 
						"$%04x: %-16s \t$%02x | $%02x | $%02x | $%02x | $%02x",
						item.LastAddr, item.LastInstr.c_str(), item.Acc, item.IndX,
						item.IndY, item.Flags, item.PtrStack);		
		ret.push(histentry);
		exechist.pop();
	}

	return ret;
}

/*
 *--------------------------------------------------------------------
 * Method:		EnableExecHistory()
 * Purpose:		Enable/disable recording of op-codes execute history.
 * Arguments:	bool - true = enable / false = disable
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void	MKCpu::EnableExecHistory(bool enexehist)
{
	mEnableHistory = enexehist;
}

/*
 *--------------------------------------------------------------------
 * Method:		IsExecHistoryEnabled()
 * Purpose:		Check if op-code execute history is enabled.
 * Arguments:	n/a
 * Returns:		bool - true = enabled / false = disabled
 *--------------------------------------------------------------------
 */
bool	MKCpu::IsExecHistoryEnabled()
{
	return mEnableHistory;
}

/*
 *--------------------------------------------------------------------
 * Method:		Reset()
 * Purpose:		Reset CPU (initiate reset sequence like in real CPU).
 *            Reset vector must point to valid code entry address.
 *            This routine will not start executing code (it is up
 *            to calling function, like from VM), but will initialize
 *            relevant flags (IRQ) and CPU registers (PC).
 *            Also, the internal flag mExitAtLastRTS will be disabled.
 *            Interrupts are by default disabled (SEI) after reset
 *            process. It is programmers responsibility to enable
 *            interrupts if IRQ/BRK is to be used during bootstrap
 *            routine. Programmer should also initialize the stack.
 *            So under the address pointed by $FFFC vector, code
 *            should reside that:
 *            - initializes stack
 *            - initializes I/O
 *            - sets arithmetic mode (CLD or SED)
 *            - enable interrupts (if needed)
 *            - jumps to the never-ending loop/main program.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void	MKCpu::Reset()
{
	mExitAtLastRTS = false;
	SetFlag(true, FLAGS_IRQ);
	mReg.PtrAddr = mpMem->Peek16bit(0xFFFC);
}

/*
 *--------------------------------------------------------------------
 * Method:		SetRegs()
 * Purpose:		Set CPU registers and flags.
 * Arguments:	Regs structure - pre-defined CPU status.
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::SetRegs(Regs r)
{
	CollapseAccQ();
	mReg.Acc = r.Acc;
	CollapseXQ();
	mReg.IndX = r.IndX;
	mReg.IndY = r.IndY;
	mReg.PtrAddr = r.PtrAddr;
	mReg.PtrStack = r.PtrStack;
	mReg.Flags &= ~FLAGS_QUANTUM;
	SetFlags(r.Flags);
}

/*
 *--------------------------------------------------------------------
 * Method:		Interrupt()
 * Purpose:		Interrupt ReQuest.
 * Arguments:	n/a
 * Returns:		n/a
 *--------------------------------------------------------------------
 */
void MKCpu::Interrupt()
{
	mReg.IrqPending = true;
}

} // namespace MKBasic
