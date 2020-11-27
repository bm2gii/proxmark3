//-----------------------------------------------------------------------------
// Copyright (C) 2012 Chalk <chalk.secu at gmail.com>
//               2015 Dake <thomas.cayrou at gmail.com>
//               2018 sguerrini97 <sguerrini97 at gmail.com>

// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Low frequency PCF7931 commands
//-----------------------------------------------------------------------------

#include "cmdlfpcf7931.h"

#include <stdio.h>
#include <string.h>
#include "comms.h"
#include "ui.h"
#include "util.h"
#include "graph.h"
#include "cmdparser.h"
#include "cmddata.h"
#include "cmdmain.h"
#include "cmdlf.h"

static int CmdHelp(const char *Cmd);

#define PCF7931_DEFAULT_INITDELAY 17500
#define PCF7931_DEFAULT_OFFSET_WIDTH 0
#define PCF7931_DEFAULT_OFFSET_POSITION 0

// Default values - Configuration
struct pcf7931_config configPcf = {
	{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
	PCF7931_DEFAULT_INITDELAY,
	PCF7931_DEFAULT_OFFSET_WIDTH,
	PCF7931_DEFAULT_OFFSET_POSITION
	};

// Resets the configuration settings to default values.
int pcf7931_resetConfig(){
	memset(configPcf.Pwd, 0xFF, sizeof(configPcf.Pwd) );
	configPcf.InitDelay = PCF7931_DEFAULT_INITDELAY;
	configPcf.OffsetWidth = PCF7931_DEFAULT_OFFSET_WIDTH;
	configPcf.OffsetPosition = PCF7931_DEFAULT_OFFSET_POSITION;
	return 0;
}

int pcf7931_printConfig(){
	PrintAndLog("Password (LSB first on bytes) : %s", sprint_hex( configPcf.Pwd, sizeof(configPcf.Pwd)));
	PrintAndLog("Tag initialization delay      : %d us", configPcf.InitDelay);
	PrintAndLog("Offset low pulses width       : %d us", configPcf.OffsetWidth);
	PrintAndLog("Offset low pulses position    : %d us", configPcf.OffsetPosition);
	return 0;
}

int usage_pcf7931_read(){
	PrintAndLog("Usage: lf pcf7931 read [h] ");
	PrintAndLog("This command tries to read a PCF7931 tag.");
	PrintAndLog("Options:");
	PrintAndLog("       h   This help");
	PrintAndLog("Examples:");
	PrintAndLog("      lf pcf7931 read");
	return 0;
}

int usage_pcf7931_write(){
	PrintAndLog("Usage: lf pcf7931 write [h] <block address> <byte address> <data>");
	PrintAndLog("This command tries to write a PCF7931 tag.");
	PrintAndLog("Options:");
	PrintAndLog("       h          This help");
	PrintAndLog("       blockaddress   Block to save [0-7]");
	PrintAndLog("       byteaddress    Index of byte inside block to write [0-15]");
	PrintAndLog("       data           one byte of data (hex)");
	PrintAndLog("Examples:");
	PrintAndLog("      lf pcf7931 write 2 1 FF");
	return 0;
}

int usage_pcf7931_bruteforce()
{
	PrintAndLog("Usage: lf pcf7931 bruteforce [h] <start password> <tries>");
	PrintAndLog("This command tries to disable PAC of a PCF7931 transponder by bruteforcing the password.");
	PrintAndLog("!! THIS IS NOT INTENDED TO RECOVER THE FULL PASSWORD !!");
	PrintAndLog("!! DO NOT USE UNLESS THE FIRST 5 BYTES OF THE PASSWORD ARE KNOWN !!");
	PrintAndLog("Options:");
	PrintAndLog("       h                This help");
	PrintAndLog("       start password   hex password to start from");
	PrintAndLog("       tries            How many times to send the same data frame");
	PrintAndLog("Examples:");
	PrintAndLog("      lf pcf7931 bruteforce 00000000123456 3");
	return 0;
}

int usage_pcf7931_config(){
	PrintAndLog("Usage: lf pcf7931 config [h] [r] <pwd> <delay> <offset width> <offset position>");
	PrintAndLog("This command tries to set the configuration used with PCF7931 commands");
	PrintAndLog("The time offsets could be useful to correct slew rate generated by the antenna");
	PrintAndLog("Caling without some parameter will print the current configuration.");
	PrintAndLog("Options:");
	PrintAndLog("       h       This help");
	PrintAndLog("       r       Reset configuration to default values");
	PrintAndLog("       pwd     Password, hex, 7bytes, LSB-order");
	PrintAndLog("       delay   Tag initialization delay (in us) decimal");
	PrintAndLog("       offset  Low pulses width (in us) decimal");
	PrintAndLog("       offset  Low pulses position (in us) decimal");
	PrintAndLog("Examples:");
	PrintAndLog("      lf pcf7931 config");
	PrintAndLog("      lf pcf7931 config r");
	PrintAndLog("      lf pcf7931 config 11223344556677 20000");
	PrintAndLog("      lf pcf7931 config 11223344556677 17500 -10 30");
	return 0;
}

int CmdLFPCF7931Read(const char *Cmd){

	uint8_t ctmp = param_getchar(Cmd, 0);
	if ( ctmp == 'H' || ctmp == 'h' ) return usage_pcf7931_read();

	UsbCommand resp;
	UsbCommand c = {CMD_PCF7931_READ, {0, 0, 0}};
	clearCommandBuffer();
	SendCommand(&c);
	if ( !WaitForResponseTimeout(CMD_ACK, &resp, 2500) ) {
		PrintAndLog("command execution time out");
		return 1;
	}
	return 0;
}

int CmdLFPCF7931Config(const char *Cmd){

	uint8_t ctmp = param_getchar(Cmd, 0);
	if ( ctmp == 0) return pcf7931_printConfig();
	if ( ctmp == 'H' || ctmp == 'h' ) return usage_pcf7931_config();
	if ( ctmp == 'R' || ctmp == 'r' ) return pcf7931_resetConfig();

	if ( param_gethex(Cmd, 0, configPcf.Pwd, 14) ) return usage_pcf7931_config();

	configPcf.InitDelay = (param_get32ex(Cmd,1,0,10) & 0xFFFF);
	configPcf.OffsetWidth = (int)(param_get32ex(Cmd,2,0,10) & 0xFFFF);
	configPcf.OffsetPosition = (int)(param_get32ex(Cmd,3,0,10) & 0xFFFF);

	pcf7931_printConfig();
	return 0;
}

int CmdLFPCF7931Write(const char *Cmd){

	uint8_t ctmp = param_getchar(Cmd, 0);
	if (strlen(Cmd) < 1 || ctmp == 'h' || ctmp == 'H') return usage_pcf7931_write();

	uint8_t block = 0, bytepos = 0, data = 0;

	if ( param_getdec(Cmd, 0, &block) ) return usage_pcf7931_write();
	if ( param_getdec(Cmd, 1, &bytepos) ) return usage_pcf7931_write();

	if ( (block > 7) || (bytepos > 15) ) return usage_pcf7931_write();

	data  = param_get8ex(Cmd, 2, 0, 16);

	PrintAndLog("Writing block: %d", block);
	PrintAndLog("          pos: %d", bytepos);
	PrintAndLog("         data: 0x%02X", data);

	UsbCommand c = {CMD_PCF7931_WRITE, { block, bytepos, data} };
	memcpy(c.d.asDwords, configPcf.Pwd, sizeof(configPcf.Pwd) );
	  c.d.asDwords[7] = (configPcf.OffsetWidth + 128);
	  c.d.asDwords[8] = (configPcf.OffsetPosition + 128);
	  c.d.asDwords[9] = configPcf.InitDelay;

	clearCommandBuffer();
	SendCommand(&c);
	//no ack?
	return 0;
}

int CmdLFPCF7931BruteForce(const char *Cmd){

	uint8_t ctmp = param_getchar(Cmd, 0);
	if (strlen(Cmd) < 1 || ctmp == 'h' || ctmp == 'H') return usage_pcf7931_bruteforce();

	uint8_t start_password[7] = {0};
	uint8_t tries = 3;

	if (param_gethex(Cmd, 0, start_password, 14)) return usage_pcf7931_bruteforce();
	if (param_getdec(Cmd, 1, &tries)) return usage_pcf7931_bruteforce();

	PrintAndLog("Bruteforcing from password: %02x %02x %02x %02x %02x %02x %02x",
			start_password[0],
			start_password[1],
			start_password[2],
			start_password[3],
			start_password[4],
			start_password[5],
			start_password[6]);

	PrintAndLog("Trying each password %d times", tries);

	UsbCommand c = {CMD_PCF7931_BRUTEFORCE, {bytes_to_num(start_password, 7), tries} };

	c.d.asDwords[7] = (configPcf.OffsetWidth + 128);
	c.d.asDwords[8] = (configPcf.OffsetPosition + 128);
	c.d.asDwords[9] = configPcf.InitDelay;

	clearCommandBuffer();
	SendCommand(&c);
	//no ack?
	return 0;
}

static command_t CommandTable[] =
{
	{"help",   CmdHelp,            1, "This help"},
	{"read",   CmdLFPCF7931Read,   0, "Read content of a PCF7931 transponder"},
	{"write",  CmdLFPCF7931Write,  0, "Write data on a PCF7931 transponder."},
	{"config", CmdLFPCF7931Config, 1, "Configure the password, the tags initialization delay and time offsets (optional)"},
	{"bruteforce", CmdLFPCF7931BruteForce, 0, "Bruteforce a PCF7931 transponder password."},
	{NULL,     NULL,               0, NULL}
};

int CmdLFPCF7931(const char *Cmd)
{
	CmdsParse(CommandTable, Cmd);
	return 0;
}

int CmdHelp(const char *Cmd)
{
	CmdsHelp(CommandTable);
	return 0;
}
