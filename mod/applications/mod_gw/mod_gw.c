
/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * "Kent Wang"<kent_wang@qq.com>
 * All Rights Reserved.
 *
 * Contributor(s):
 * 
 * "Kent Wang"<kent_wang@qq.com>
 *
 * mod_gw.c -- Octon Media Server API
 *
 */

/*
¥Í¦¨ freeswitch ¨Æ¥óªº´XºØ¤èªk
1.²£¥Í¤º¸m¨Æ¥ó ¡]¥HSWITCH_EVENT_MODULE_LOAD¬°¨Ò):
switch_event_t *event;
if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS)
{
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "endpoint");
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
    switch_event_fire(&event);
}

2. ²£¥Í¦Û©w¸q¨Æ¥ó;
if (switch_event_create_subclass(&event,SWITCH_EVENT_CUSTOM,"calltest1::calltest1_sub") == SWITCH_STATUS_SUCCESS)
{
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "callee_uuid", "86896a7a-3dc3-4175-aaa1-cdcbfd9bd566");
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "caller_num", "1000");
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "callee_num", "1001");
    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "failed_reason", "exten not avaliable");
    switch_event_fire(&event);
}

switch_event_destroy(&event);
*/

#include <switch.h>
#include <switch_curl.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
SWITCH_MODULE_LOAD_FUNCTION(mod_gw_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_gw_shutdown);
SWITCH_MODULE_DEFINITION(mod_gw, mod_gw_load, mod_gw_shutdown, NULL);
// SWITCH_MODULE_DEFINITION_EX(name, load, shutdown, runtime, SMODF_NONE)

#define GW_VERSION "1.0.0"

#define MII55 1
#define MII60 1
#define MII81 1
#define MII82 1 
#define MII86 1  // change config file 
#define MII90 1 
#define MII94 1 
#define MII98 1
#define MII100 1 // to search RFC2833_LOST_END in switch_rtp.c:639 - 649 
#define MII102 1
#define MII103 1
#define MII104 1
#define MII130 1 // about monitor
#define MII131 1 
#define MII132 1 
#define MII53 1 
#define MII149 1	// 2020/5/13
// 20b2
// DTMF inbound & rtp event ²VÂø

#define GW_HELLOWORLD "\nGW Start "GW_VERSION" build at "__DATE__" "__TIME__"\n"
#define MAX_PLAYLIST 512
#define MAX_PLAYFILELEN 128
#define FAX_DIR	"/Data/Fax"

enum GW_RECORD_MODE{
	GW_REC_SYNC,
	GW_REC_BLOCK
};

struct _recording_s{
	int	 Rec_Mode;			// 2019/12/6 0: GW_REC_SYNC, 1:GW_REC_BLOCK 
	char Rec_Type[16];
	//char RecordFile[150];
	//char RecordPath[100];
	//char TempPathFile[250];

	char RecordFile[512];
	char RecordPath[512];
	char TempPathFile[1024];

	char TermKey[20];
	int Stereo;
	int OnlyCallLeg;
	int Duration;
	int Silence;
	int isBlockRecording;
};


typedef struct _recording_s gw_recording_t;

typedef struct gw_call_leg_s{
	char *name;
	//starttime
	switch_time_t ts_start;
	//UUID for session
	char * session_uuid;

	char *session_sn;	// peter
	//Call-ID 
	char * call_id;
	char *call_name;
	//status: 0:offline 1:idle 2:
	int status; 

	int channel_active;		// peter 2023/8/1
	//Conference number ;
	int room_num;
	int old_room_num;

	// member_id
	int member_id;
	char str_member_id[64];
	//  
	int InitialTimeOut_ReturnCode;
	int InterDigitTimeOut_ReturnCode;
	int DigitMask_ReturnCode;
	int InterDigitTimeOut;
	int InitialTimeOut;

	int ASR;
	char ASRType[32];
	char ASRStatus[32];
	char ASRText[256];
	char ASRResult[512];

	int MinKey;
	char Domain[64];
	int ClearBuffer;
	int BargeIn;
	char TermKey[32];
	int RepeatPlay;
	int UsingDtmf;
	int Security;	// peter 2019/8/21
	char DigitMask[32];
	int  ReturnEvt;
	int  nFileList;
	char FileList[MAX_PLAYLIST][MAX_PLAYFILELEN];
	char LanguageType[32];
	char MySound_prefix[256];
	int  MaxKey;
	char LastReq[64];
	int  PlayAll;
	unsigned long dtmf_start;
	char last_input[2];
	int  File_idx;

	int  play_error;
	char error_msg[512];

	int  dtmf_idx;
	char dtmf_buf[128];

	int  play_req_sn;
	char play_cookie[64];

	switch_time_t dtmf_Initial_ts;
	switch_time_t InterDigit_ts;

	char * want_dtmf_call_id; // for conf

	int ref;
	switch_memory_pool_t *pool;

	char  coach_id[256]; //coach list
	char  nohear_id[256]; //coach list

	char * contact_uri;
	char * from_uri;
	char * to_uri;
	char * x_call_id;
	int  wait_reinvite;
	switch_event_t * wait_cmds; 
	int volume_out_flag;
	int volume_out;
	int dtmf_type;
	int dtmf_duration;	// peter
	int generate_dtmf_tone;	// peter 2019/10/1

	// for block recording
	char RecTermKey[20];	// peter 2019/12/4 block ¿ý­µ®É¥i¤¤Â_ªº«öÁä

	char *pRecordFile;	// «ü¦V  gw_recording_t ªº RecordFile ¥H°O¿ý block recording ªº RecordFile
	char *pRecordPath;	// ¦P¤W
	char *pRecordType;	// ¦P¤W, 2022/10/7

	switch_time_t dtmf_last_ts;		// peter 2020/9/2
	char dtmf_last[2];				// peter 2020/9/2
	int  dtmf_ignore_ms;			// peter 2020/9/2

	int  send_dtmf_event;			// peter 2022/6/10

	int is_ai_service;
	int active_audio_stream;		// peter 2024/1/17
	int tts_index;
	char sip_ANI[64];
	char sip_DNIS[64];
}gw_call_leg_t;

enum GW_CL_STATUS{
	//when recieve add_mem event ,will be set 1 
	GW_CL_STATUS_READY=1 ,
	GW_CL_STATUS_PLAY=2,
	GW_CL_STATUS_MERGE=4,
	GW_CL_STATUS_MUTE=8,
	GW_CL_STATUS_COACH=16,
	GW_CL_STATUS_NOT=32,
	GW_CL_STATUS_TIMER=64,
	GW_CL_STATUS_DEAF=128,
	GW_CL_STATUS_BLOCK_RECORD=256,
	GW_CL_STATUS_BREAK_PLAY=512	// 2019/8/1 add by peter

};

enum GW_PLAY_CODE{
	GW_PLAY_CODE_DONE,
	GW_PLAY_CODE_TERM_KEY,
	GW_PLAY_CODE_MAX_KEY,
	GW_PLAY_CODE_INPUT_TIMEOUT,
	GW_PLAY_CODE_INPUT_DIGIT_TIMEOUT,
	GW_PLAY_CODE_KEY_MISS_MASK,
	GW_PLAY_CODE_PLAY_ERROR,
	GW_STOP_PLAY_CURRENT,
	GW_STOP_PLAY_ALL
};


#define GW_CL_STATUS_IS(c,v)  ( (c)->status & (v) ) 
#define GW_CL_STATUS_SET(c,v)  ( (c)->status |= (v) ) 
#define GW_CL_STATUS_CLS(c,v)  ( (c)->status &= ~(v) ) 


#define GW_CL_STATUS_IS_READY(c)  ( (c)->status & GW_CL_STATUS_READY ) 
#define GW_CL_STATUS_SET_READY(c)  ( (c)->status |= GW_CL_STATUS_READY ) 
#define GW_CL_STATUS_CLS_READY(c)  ( (c)->status &= ~GW_CL_STATUS_READY ) 

#define GW_CL_STATUS_IS_PLAYING(c)  ( (c)->status & GW_CL_STATUS_PLAY ) 
#define GW_CL_STATUS_IS_MERGE(c)  ( (c)->status & GW_CL_STATUS_MERGE ) 
#define GW_CL_STATUS_IS_MUTE(c)  ( (c)->status & GW_CL_STATUS_MUTE ) 
#define GW_CL_STATUS_IS_COACH(c)  ( (c)->status & GW_CL_STATUS_COACH ) 

// #define GW_CL_STATUS_SET_PLAYING(c)  ( (c)->status |= GW_CL_STATUS_PLAY ) 
// #define GW_CL_STATUS_SET_MERGE(c)  ( (c)->status |= GW_CL_STATUS_MERGE ) 
// #define GW_CL_STATUS_SET_MUTE(c)  ( (c)->status |= GW_CL_STATUS_MUTE ) 
// #define GW_CL_STATUS_SET_COACH(c)  ( (c)->status |= GW_CL_STATUS_COACH ) 

#define GW_CL_STATUS_CLS_PLAYING(c)  ( (c)->status &= ~GW_CL_STATUS_PLAY ) 
// #define GW_CL_STATUS_CLS_MERGE(c)  ( (c)->status &= ~GW_CL_STATUS_MERGE ) 
#define GW_CL_STATUS_CLS_MUTE(c)  ( (c)->status &= ~GW_CL_STATUS_MUTE )		// 2023/7/31
// #define GW_CL_STATUS_CLS_COACH(c)  ( (c)->status &= ~GW_CL_STATUS_COACH ) 

enum GW_RETURN_CODE{ 
	GW_RETURN_OK,
	GW_RETURN_RETRY,
	GW_RETURN_ERROR_NOFOUND
}; 

typedef struct gw_conference_room_s{
	char name[64];
	int room_num;
	int ref;
	switch_event_t * members; 
}gw_conference_room_t;

typedef int (*eventproc_func_t)(switch_event_t *event,int argc,void * argv[]);

typedef struct gw_eventproc_s{
	int e_event_id;
	char * e_subclass_name;
	char * key ;
	char * value; 
	eventproc_func_t func;
}gw_eventproc_t;

#define MAX_EVENTPROC_NUM 64
#define MAX_HA_COUNT 8
static struct {
	switch_memory_pool_t *pool;

	switch_queue_t *event_queue;
	switch_queue_t *event_retry_queue;
	switch_queue_t *event_tmp_queue;
	switch_mutex_t *mutex;
	switch_mutex_t *callegs_mutex;	
	switch_hash_t * callegs_hash; // K=>Call-Id V:{} 
	switch_hash_t * callegs_timeout_hash; // K=>Call-Id V:{} 

	switch_mutex_t *recording_mutex;	// peter 2019/10/8	
	switch_hash_t * recording_hash; // K=>RecordFile V:{}		// peter 2019/10/8

	int num_threads;
	int max_threads;
	int wait_messages;

	int debug;
	int running;
	
	int next_room_num;

	switch_hash_t * room_hash; // K=>Call-Id V:{} 
	switch_queue_t *room_queue;

	gw_eventproc_t eventprocs[MAX_EVENTPROC_NUM];
	int num_eventprocs;
#ifdef MII55
	int ha_argc;
	char * ha_argv[MAX_HA_COUNT];
	switch_time_t ha_time1[MAX_HA_COUNT];
	switch_time_t ha_time2[MAX_HA_COUNT];
#endif 
	int sendingfax;

	// switch_mutex_t *room_mutex;	// peter 2022/12/15 °Ê§@À³¸Ó¬O¨Ìªþ¦b mutex ¤º, ¥i¯à¤£»Ý­n¥t¥~ªº mutex
	int room_in_queue[4096];		// peter 2022/12/15 °O¿ý©Ð¶¡ 6000 ~ 6000 +2048+2048 in queue ªºª¬ºA

} globals;

static const char * IVR_PLAY_PROMPT_EVT_FMT1="{ "
	"\"Name\":\"IVR_PLAY_PROMPT_EVT\"," 
	"\"SessionID\":\"%s\","
	"\"INPUTDTMF\":\"%s\","
	"\"USERINPUT\":\"%s%s\","
	"\"EOF\":\"%s\","
	"\"TERMKEY\":\"%s\","
	"\"AUDIOERROR\":\"%s\"}"; 
static const char * IVR_PLAY_PROMPT_EVT_FMT2="{ "
	"\"Name\":\"IVR_PLAY_PROMPT_EVT\"," 
	"\"SessionID\":\"%s\","
	"\"Security\":\"true\","
	"\"INPUTDTMF\":\"%s\","
	"\"USERINPUT\":\"%s%s\","
	"\"EOF\":\"%s\","
	"\"TERMKEY\":\"%s\","
	"\"AUDIOERROR\":\"%s\"}"; 

static const char * IVR_PLAY_PROMPT_EVT_ASRFMT="{ "
	"\"Name\":\"IVR_PLAY_PROMPT_EVT\"," 
	"\"SessionID\":\"%s\","
	"\"Security\":\"true\","
	"\"INPUTDTMF\":\"%s\","
	"\"USERINPUT\":\"%s%s\","
	"\"EOF\":\"%s\","
	"\"TERMKEY\":\"%s\","
	"\"AUDIOERROR\":\"%s\"," 
	"\"ASR\":\"%s\","
	"\"ASR_status\":\"%s\","
	"\"ASR_text\":\"%s\","
	"\"ASR_result\":\"%s\"}"; 

static const char * DOES_NOT_EXIST_EVT_FMT0="{ "
	"\"CallID\":\"%s\","
	"\"Name\":\"DOES_NOT_EXIST_EVT\"}\n";

static const char * RECORD_CALL_EVT_FMT0="{ "
	"\"Type\":\"%s\","
	"\"RecordFile\":\"%s\","
	"\"CallID\":\"%s\","
	"\"Duration\":%d,"
	"\"RecordPath\":\"%s\","
	"\"Stereo\":%s,"
	"\"OnlyCallLeg\":%s,"
	"\"Name\":\"RECORD_CALL_EVT\"}";
//[#000118]mod_gw.c:2277 RECV NOTIFY :  {"request":{"Type":"wav","RecordFile":"20191007093748290_iSwitch_2067_5102_6cfc50-b300a8c0-13c4-55013-5d9a96e6-36d63cf1-5d9a96e6",
// "CallID":"6cfc50-b300a8c0-13c4-55013-5d9a96e6-36d63cf1-5d9a96e6","Duration":20,"RecordPath":"/Data/RecordingFiles/SpeakerVerification/","Stereo":false,"OnlyCallLeg":true,"Name":"RECORD_CALL_REQ"}}

static const char * RECORD_CALL_EVT_FMT1="{ "
	"\"TerminationKey\":\"%s\","
	"\"Type\":\"%s\","
	"\"RecordFile\":\"%s\","
	"\"CallID\":\"%s\","
	"\"Duration\":%d,"
	"\"RecordPath\":\"%s\","
	"\"Stereo\":%s,"
	"\"OnlyCallLeg\":%s,"
	"\"Name\":\"RECORD_CALL_EVT\"}";


static const char * DTMF_PRESSED_EVT_FMT0="{ "
	"\"Name\":\"DTMF_PRESSED_NOTI\","
	"\"SessionID\":\"%s\","
	"\"INPUTDTMF\":\"%c\"}";

#define SWITCH_EVENT_SUBCLASS_CONFERENCE "conference::maintenance"
#define SWITCH_EVENT_SUBCLASS_GW "gw::control"
#define GW_EVENT_QUEUE_SIZE 50000
#define GW_DESC "Octon media engine"
#define GW_USAGE ""
static void gw_conference_execute_cmd(gw_call_leg_t* cl,int flag,const char *cmd,switch_core_session_t * session);
//static void gw_call_execute_cmd(gw_call_leg_t* cl,const char *cmd,const char *arg,switch_core_session_t * session);
static void gw_call_leg_dtmf_handler(gw_call_leg_t * cl,const char * key,int idtmf_type);

static int actual_message_query_handler(switch_event_t *event,int argc,void * argv[]);

#define GW_SYNTAX "[debug_on|debug_off]"

// char oldmsg[10000]; 


struct camping_stake {
	switch_core_session_t *session;
	int running;
	int do_xfer;
	const char *moh;
};

// #define SWITCH_STANDARD_API(name) static switch_status_t name (_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream)
// static switch_status_t gw_api_debug_function(_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream)
SWITCH_STANDARD_API(gw_api_debug_function)
{
	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (zstr(cmd)) {
		goto usage;
	}

	if (!strcasecmp(cmd, "debug_on")) {
		globals.debug = 1;
	} else if (!strcasecmp(cmd, "debug_off")) {
		globals.debug = 0;
	} else {
		goto usage;
	}

	stream->write_function(stream, "OK\n");
	return SWITCH_STATUS_SUCCESS;

  usage:
	stream->write_function(stream, "USAGE: %s\n", GW_SYNTAX);
	return SWITCH_STATUS_SUCCESS;
}

static void bind_eventproc(int id, const char * sub, const char * k,const char *v, eventproc_func_t handler)
{
	// Number 3..... 
	// plog("coming to bind_eventproc\n");

	if(globals.num_eventprocs < MAX_EVENTPROC_NUM){
		gw_eventproc_t * ep = &globals.eventprocs[globals.num_eventprocs];
		ep->e_event_id = id;														// SWITCH_EVENT_CUSTOM
		ep->e_subclass_name=sub ? switch_core_strdup(globals.pool,sub):NULL;		// SWITCH_EVENT_SUBCLASS_GW / SWITCH_EVENT_SUBCLASS_CONFERENCE
		ep->key = k ? switch_core_strdup(globals.pool,k):NULL;						// "Action"
		ep->value=v ? switch_core_strdup(globals.pool,v):NULL;						// "IVR_PLAY_PROMPT_REQ" / "add-member"
		ep->func=handler;															// gw_ev_play_prompt_handler / gw_ev_add_member_handler
		globals.num_eventprocs++;

	}else{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"num_eventprocs > %d\n",MAX_EVENTPROC_NUM);
	}
}

void gw_fill_room(int num)
{
	int i;

	// Number 2
	// plog("coming to gw_fill_room(%d)\n", num);

	for(i=0; i<num; i++)
	{
		gw_conference_room_t * pr = (gw_conference_room_t*)switch_core_alloc(globals.pool, sizeof(gw_conference_room_t));

		if(pr)
		{
			/*
			typedef struct gw_conference_room_s{
				char name[64];
				int room_num;
				int ref;
				switch_event_t * members; 
			} gw_conference_room_t;
			*/

			pr->members = NULL;		// switch_event_t *
			pr->ref = 0;			// int

			pr->room_num=globals.next_room_num;		// int
			globals.next_room_num++;

			sprintf(pr->name, "%d", pr->room_num);	// char[64]

			switch_mutex_lock(globals.mutex);
			switch_core_hash_insert(globals.room_hash, pr->name, pr);
			switch_queue_push(globals.room_queue, pr);

			// 2022/12/15
			if(pr->room_num>=0 && pr->room_num < 10096)	// 6000+2048+2048
			{
				globals.room_in_queue[pr->room_num-6000] = 1;
			}
			switch_mutex_unlock(globals.mutex);
		}
		else
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"NOT ALLOC CONFERENCE ROOM\n");
			break;			
		}
	}
}

gw_conference_room_t * gw_pop_room()
{
	gw_conference_room_t * proom=NULL;
	int flag = 0;

	// plog("coming to gw_pop_room\n");

	switch_mutex_lock(globals.mutex);		

again:
	switch_queue_trypop(globals.room_queue, (void**)&proom);
	
	if(  proom ==NULL ) {
		gw_fill_room(2048);
		switch_queue_trypop(globals.room_queue, (void**)&proom);
	}

	// peter 2022/12/15
	if(proom->room_num >=0 && proom ->room_num < 10096)	// 6000+2048+2048
	{
		if(globals.room_in_queue[proom->room_num-6000] == 0)	// ³o¸¹½X¤w¸g®³¥X¨Ó¦b¥Î¤F
		{
			flag = proom->room_num;
			goto again;			
		}
		else
			globals.room_in_queue[proom->room_num-6000] = 0;	// ±q Queue ¤º§ì¥X¨Ó´N²M°£°O¸¹

	}

	switch_mutex_unlock(globals.mutex);		

	if(flag)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Room %d is already in use, don't pop this room again.\n", flag);
	}

	return proom;
}

void gw_push_room(gw_conference_room_t * pr)
{
	int flag = 0;
	// plog("coming to gw_push_room\n");

	switch_mutex_lock(globals.mutex);	
	if(pr->room_num>=0 && pr->room_num < 10096)	// 6000+2048+2048
	{
		if(globals.room_in_queue[pr->room_num-6000] == 0)
		{
			switch_queue_push(globals.room_queue,pr);
			globals.room_in_queue[pr->room_num-6000] = 1;	// ¥á¨ì Queue ¤º®É°µ°O¸¹, ¥H§K­«ÂÐ¥á
		}
		else
			flag = pr->room_num;
	}
	else
		switch_queue_push(globals.room_queue,pr);	// ¤£¥i¯àµo¥Í
	
	switch_mutex_unlock(globals.mutex);

	if(flag)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Room %d is already in queue, don't push again.\n", flag);
	}

}

gw_conference_room_t * gw_find_room(int r)
{
	char room_str[64];
	gw_conference_room_t * room=NULL;

	// plog("coming to gw_find_room\n");
	sprintf(room_str,"%d",r);
	switch_mutex_lock(globals.mutex);
	room =switch_core_hash_find(globals.room_hash,room_str);
	switch_mutex_unlock(globals.mutex);

	return room;
}

int gw_get_room_member_count(int r)
{
	char room_str[64];
	int ret=0;
	gw_conference_room_t * room=NULL;

	// plog("coming to gw_get_room_member_count\n");
	sprintf(room_str,"%d",r);
	switch_mutex_lock(globals.mutex);
	room =switch_core_hash_find(globals.room_hash,room_str);
	switch_mutex_unlock(globals.mutex);

	if(room && room->members ){
		switch_event_header_t *header = NULL;
		for (header = room->members->headers; header; header = header->next) {
			ret++;
		}
	}

	return ret;
}

const char * str_list_add(char * str, const char * key)
{
	if(str[0])
		strcat(str,",");

	strcat(str,key);

	return str;
}

int str_list_in(const char *str,const char*key)
{
	char * p = strstr(str,key);

	if(p){
		int len =strlen(key);
		if(p[len]==',' || p[len]==0x0)
			return 1;
	}

	return 0;
}
const char * str_list_del(char * str,const char * key)
{
	char * p = strstr(str,key);

	if(p){
		int len =strlen(key);
		if(p[len]==',')
		{
			strcpy(p,p+len+1); 
		}else if(p[len]==0x0)
		{
			// last one
			*p=0x0;
		}
	}	

	return str;
}

int member_get_strangers(gw_call_leg_t* cl1,gw_call_leg_t* cl2)
{
	switch_event_header_t *header = NULL;

	gw_conference_room_t * room = gw_find_room(cl1->room_num);

	// plog("coming to member_get_strangers\n");

	if(!room || !room->members) return 0 ;
	if(cl2)
		str_list_add(cl1->coach_id,cl2->str_member_id);
	cl1->nohear_id[0]=0;
	for (header = room->members->headers; header; header = header->next) {
		if(strcmp(header->name,cl1->call_id)){
			//ä¸æ˜¯?³è¯­??
			if(str_list_in(cl1->coach_id,header->value) ==0 )
			{
				//ä¹Ÿä??¯è¢«?³è¯­??
				//æ·»å??°ä??½å¬è§ç??¬è€?				str_list_add(cl1->nohear_id,header->value);
			}
		}
	}

	return strlen(cl1->nohear_id);
}


gw_conference_room_t * gw_find_room_by_event(switch_event_t *event)
{
	gw_conference_room_t * room=NULL;
	const char * name=switch_event_get_header(event, "Conference-Name");

	// plog("coming to gw_find_room_by_event\n");
	if(name){
		switch_mutex_lock(globals.mutex);		
		room =switch_core_hash_find(globals.room_hash,name);
		switch_mutex_unlock(globals.mutex);		
	}

	return room;
}

gw_call_leg_t * gw_find_callleg(const char *gw_call_id)
{
	gw_call_leg_t * cl =NULL;

	// plog("coming to gw_find_callleg\n");
	switch_mutex_lock(globals.callegs_mutex);		
	cl = switch_core_hash_find(globals.callegs_hash,gw_call_id);
	switch_mutex_unlock(globals.callegs_mutex);

	return cl;		
}

void  gw_rm_callleg(const char *gw_call_id)
{
	// plog("coming to gw_rm_callleg\n");

	switch_mutex_lock(globals.callegs_mutex);		
	switch_core_hash_delete(globals.callegs_hash,gw_call_id);
	switch_core_hash_delete(globals.callegs_timeout_hash,gw_call_id);
	switch_mutex_unlock(globals.callegs_mutex);
}


gw_recording_t * gw_find_recording(const char *gw_recordfile)
{
	gw_recording_t * rec =NULL;

	switch_mutex_lock(globals.recording_mutex);		
	rec = switch_core_hash_find(globals.recording_hash, gw_recordfile);
	switch_mutex_unlock(globals.recording_mutex);

	return rec;		
}

void  gw_rm_recording(const char *gw_recordfile)
{

	switch_mutex_lock(globals.recording_mutex);		
	switch_core_hash_delete(globals.recording_hash, gw_recordfile);
	switch_mutex_unlock(globals.recording_mutex);
}

// #define SWITCH_STANDARD_APP(name) static void name (switch_core_session_t *session, const char *data)

/*
SWITCH_STANDARD_APP(gw_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_event_t *event;
 	switch_event_header_t *hp;
	
	// calling to gw

//	if (switch_channel_answer(channel) != SWITCH_STATUS_SUCCESS) {
//		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Channel answer failed.\n");
//		goto end;
//	}

	
	switch_channel_get_variables(channel, &event);
	if (event) 
	{
		
		for (hp = event->headers; hp; hp = hp->next) 
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "event, name = %s, val = %s\n", hp->name, hp->value);
		}
	}
	switch_event_destroy(&event);

	
		
		name = direction, val = inbound
		name = uuid, val = 63e2e908-aa09-4478-84d8-fb96626e2ea6
		name = session_id, val = 1
		name = sip_from_user, val = 2096.iSwitch
		name = sip_from_uri, val = 2096.iSwitch@172.16.0.92
		name = sip_from_host, val = 172.16.0.92
		name = video_media_flow, val = sendrecv
		name = channel_name, val = sofia/internal/2096.iSwitch@172.16.0.92
		name = ep_codec_string, val = CORE_PCM_MODULE.PCMA@8000h@20i@64000b,CORE_PCM_MODULE.PCMU@8000h@20i@64000b
		name = sip_local_network_addr, val = 172.16.0.92
		name = sip_network_ip, val = 172.16.0.92
		name = sip_network_port, val = 5060
		name = sip_received_ip, val = 172.16.0.92
		name = sip_received_port, val = 5060
		name = sip_via_protocol, val = udp
		name = sip_authorized, val = true
		name = sip_acl_authed_by, val = domains
		name = sip_from_user_stripped, val = 2096.iSwitch
		name = sofia_profile_name, val = internal
		name = recovery_profile_name, val = internal
		name = sip_req_user, val = 2095
		name = sip_req_port, val = 5070
		name = sip_req_uri, val = 2095@172.16.0.92:5070
		name = sip_req_host, val = 172.16.0.92
		name = sip_to_user, val = 2095
		name = sip_to_port, val = 5060
		name = sip_to_uri, val = 2095@172.16.0.92:5060
		name = sip_to_host, val = 172.16.0.92
		name = sip_contact_params, val = transport=udp
		name = sip_contact_user, val = 2096.iSwitch
		name = sip_contact_port, val = 5060
		name = sip_contact_uri, val = 2096.iSwitch@172.16.0.92:5060
		name = sip_contact_host, val = 172.16.0.92
		name = sip_user_agent, val = AUDC-IPPhone/2.2.2.77 (420HD-Rev1; 00908F5633B1)
		name = sip_via_host, val = 172.16.0.92
		name = sip_via_port, val = 5060
		name = max_forwards, val = 69
		name = presence_id, val = 2096.iSwitch@172.16.0.92
		name = switch_r_sdp, val = v=0
			o=- 1552964952 1552964952 IN IP4 172.16.0.92
			s=-
			c=IN IP4 172.16.0.92
			t=0 0
			m=audio 21738 RTP/AVP 8 0 101
			a=rtpmap:8 PCMA/8000
			a=rtpmap:0 PCMU/8000
			a=rtpmap:101 telephone-event/8000
			a=fmtp:101 0-15

		name = call_uuid, val = 63e2e908-aa09-4478-84d8-fb96626e2ea6
		name = rtp_info_when_no_2833, val = false
		name = audio_media_flow, val = sendrecv
		name = rtp_use_codec_string, val = OPUS,G722,G729,PCMU,PCMA,VP8,H264,H263,H263-1998,G7221@32000h
		name = rtp_audio_recv_pt, val = 8
		name = rtp_use_codec_name, val = PCMA
		name = rtp_use_codec_rate, val = 8000
		name = rtp_use_codec_ptime, val = 20
		name = rtp_use_codec_channels, val = 1
		name = rtp_last_audio_codec_string, val = PCMA@8000h@20i@1c
		name = read_codec, val = PCMA
		name = original_read_codec, val = PCMA
		name = read_rate, val = 8000
		name = original_read_rate, val = 8000
		name = write_codec, val = PCMA
		name = write_rate, val = 8000
		name = dtmf_type, val = rfc2833
		name = local_media_ip, val = 172.16.0.92
		name = local_media_port, val = 51268
		name = advertised_media_ip, val = 172.16.0.92
		name = rtp_use_timer_name, val = soft
		name = rtp_use_pt, val = 8
		name = rtp_use_ssrc, val = 1486100752
		name = rtp_2833_send_payload, val = 101
		name = rtp_2833_recv_payload, val = 101
		name = remote_media_ip, val = 172.16.0.92
		name = remote_media_port, val = 21738
		name = rtp_local_sdp_str, val = v=0
			o=FreeSWITCH 1552913228 1552913229 IN IP4 172.16.0.92
			s=FreeSWITCH
			c=IN IP4 172.16.0.92
			t=0 0
			m=audio 51268 RTP/AVP 8 101
			a=rtpmap:8 PCMA/8000
			a=rtpmap:101 telephone-event/8000
			a=fmtp:101 0-16
			a=silenceSupp:off - - - -
			a=ptime:20
			a=sendrecv

		name = endpoint_disposition, val = ANSWER
		name = sip_to_tag, val = SH0cQFBr63j7j
		name = sip_from_tag, val = 6ebe20-7e0010ac-13c4-55013-5c905b8f-6d6bf536-5c905b8f
		name = sip_cseq, val = 1
		name = sip_call_id, val = 6f8740-7e0010ac-13c4-55013-5c905b8f-3c398710-5c905b8f
		name = sip_invite_record_route, val = <sip:172.16.0.92:5060;transport=udp>;lr
		name = sip_full_via, val = SIP/2.0/UDP 172.16.0.92:5060;branch=z9hG4bK51255b4a839c4fda31c629638d6c154a,SIP/2.0/UDP 172.16.0.126:5060;rport=5060;branch=z9hG4bK-5c905b8f-93e5a8cd-1786b4cd;received=172.16.0.126
		name = sip_from_display, val = 2096.iSwitch
		name = sip_full_from, val = "2096.iSwitch" <sip:2096.iSwitch@172.16.0.92>;tag=6ebe20-7e0010ac-13c4-55013-5c905b8f-6d6bf536-5c905b8f
		name = sip_full_to, val = <sip:2095@172.16.0.92:5060>;tag=SH0cQFBr63j7j
		name = min_dup_digit_spacing_ms, val = 40
		name = current_application, val = gw
		
		

	
	if(!switch_channel_get_variable(channel,"gw_touch"))
	{
		char conference_num[128];
		gw_conference_room_t * room=gw_pop_room();
		
		if(room)
		{
			gw_call_leg_t * cl=NULL;
			switch_memory_pool_t *pool=NULL;

			switch_channel_set_variable(channel,"gw_touch","gw");
			switch_channel_set_variable(channel,"conference_enforce_security","yes");
			switch_channel_set_variable(channel,"conference_silent_entry","yes");
			switch_channel_set_app_flag_key("conference_silent", channel, 2);
			
			switch_channel_set_variable_printf(channel,"sound_prefix","%s",SWITCH_GLOBAL_dirs.sounds_dir);

			//
			sprintf(conference_num,"%d@gw",room->room_num);
			switch_channel_set_variable_printf(channel,"gw_conference_num","%d",room->room_num);

			// 2022/7/8
			switch_channel_set_variable(channel, "fire_asr_events", "true");

			switch_core_new_memory_pool(&pool);

			cl=pool ? (gw_call_leg_t*)switch_core_alloc(pool,sizeof(gw_call_leg_t)) : NULL;

			if(cl)
			{
				memset(cl, 0, sizeof(gw_call_leg_t));

				cl->pool = pool;
				cl->call_id = switch_core_strdup(pool, switch_channel_get_variable(channel, "sip_call_id"));
				cl->session_uuid = switch_core_strdup(pool, switch_channel_get_uuid(channel));
				cl->session_sn = switch_core_strdup(pool, switch_channel_get_sn(channel));
				cl->call_name=switch_core_strdup(pool, switch_channel_get_name(channel));
				cl->ts_start=switch_time_ref();
				cl->room_num = room->room_num;
				cl->name = switch_core_strdup(pool, switch_channel_get_name(channel));

				cl->wait_reinvite=0;
				if(strcmp("0.0.0.0",switch_channel_get_variable(channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE))==0){		// remote_media_ip
					cl->wait_reinvite=1;
				}
				
				// alloc conference room id
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE,"New iHMP %s Call-ID:%s NEW Unique-ID:%s Room:%s sound_prefix:%s/domain wait_reinvite:%d",
				//	cl->call_name,cl->call_id,cl->session_uuid ,conference_num,SWITCH_GLOBAL_dirs.base_dir,cl->wait_reinvite);

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,"New iHMP Room:%s sound_prefix:%s/domain wait_reinvite:%d",
					conference_num,SWITCH_GLOBAL_dirs.base_dir,cl->wait_reinvite);


				switch_channel_set_variable_printf(channel,"gw_call_id","%s",cl->call_id);

				cl->dtmf_type=SWITCH_DTMF_INBAND_AUDIO;
//				gw_call_execute_cmd(cl,"start_dtmf_generate","wirte",session);

				// 2020/9/22 peter
				cl->dtmf_last_ts = 0;	// clean
				cl->dtmf_last[0] = 0;	// clean
				cl->dtmf_ignore_ms =0;	// clean

				cl->send_dtmf_event = 0;	// 2022/6/10 clean

				cl->channel_active = 0;		// 2023/8/1

				cl->is_ai_service = 0;
				cl->active_audio_stream = 0;	// 2024/1/17
				cl->tts_index = 0;			// 2024/2/2

#ifdef MII60 
				{
					const char * dtmf_type= switch_channel_get_variable(channel,"dtmf_type");
					const char * r_sdp =  switch_channel_get_variable(channel,"switch_r_sdp");

					if( dtmf_type && strcmp(dtmf_type,"rfc2833")==0)
						cl->dtmf_type=SWITCH_DTMF_RTP;

					if(cl->dtmf_type!= SWITCH_DTMF_RTP && strstr(r_sdp,"telephone-event/"))
						cl->dtmf_type=SWITCH_DTMF_RTP;

					// if(cl->dtmf_type==SWITCH_DTMF_INBAND_AUDIO){
					// 	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE,
					// 		"iHMP %s start Inband DTMF detect and Outbound DTMF generate\n",cl->call_name);
					// 	gw_call_execute_cmd(cl,"set","min_dup_digit_spacing_ms=40",session);							
					// 	gw_call_execute_cmd(cl,"spandsp_start_dtmf","",session);
					// }
				}
#endif 
				switch_mutex_lock(globals.callegs_mutex);
				
				switch_core_hash_insert(globals.callegs_hash, cl->call_id, cl);	// sip_call_id

				switch_mutex_unlock(globals.callegs_mutex);

				switch_core_session_execute_application(session, "conference", conference_num);		// trigger app conference() -> gw_ev_add_member_handler()
			}
			else
			{
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(switch_channel_get_uuid(channel)), SWITCH_LOG_ERROR," iHMP NOT alloc Call Call-ID:%s\n", switch_channel_get_variable(channel, "sip_call_id"));
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR," iHMP NOT alloc Call Call-ID:%s\n", switch_channel_get_variable(channel, "sip_call_id"));	// peter
			}
		}
		else
		{
			// 
			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(switch_channel_get_uuid(channel)), SWITCH_LOG_ERROR," iHMP NOT Coference Room Call-ID:%s\n", switch_channel_get_variable(channel, "sip_call_id"));
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR," iHMP NOT Coference Room Call-ID:%s\n", switch_channel_get_variable(channel, "sip_call_id"));	// peter
		}
	}
	else
	{
		// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(switch_channel_get_uuid(channel)), SWITCH_LOG_ERROR," iHMP Call repeat Call-ID:%s\n",	switch_channel_get_variable(channel, "sip_call_id"));
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR," iHMP Call repeat Call-ID:%s\n",	switch_channel_get_variable(channel, "sip_call_id"));	// peter
	}
	

	return;
}
*/

static switch_status_t bridge_on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	char *str = (char *) buf;

	if (str && input && itype == SWITCH_INPUT_TYPE_DTMF) {
		switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
		if (strchr(str, dtmf->digit)) {
			return SWITCH_STATUS_BREAK;
		}
	}
	return SWITCH_STATUS_SUCCESS;
}


static void send_kek_req(char *call_id)
{
	switch_event_t *kek_event;
	switch_time_t now_ts;
	char *hostname;
	char *local_ip;
	char *ip_port;
	char *kek_iswitch;
	char *kek_enable;
	char buffer[1024];
	

	hostname = switch_core_get_variable("hostname");
	local_ip = switch_core_get_variable("local_ip_v4");
	ip_port = switch_core_get_variable("internal_sip_port");
	kek_iswitch = switch_core_get_variable("kek_iswitch_ip_port");
	kek_enable = switch_core_get_variable("kek_enable");

	if (zstr(kek_iswitch))
		return;

	now_ts = switch_time_ref();

	if(kek_enable && switch_true(kek_enable))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"enable kek\n");
		if(switch_event_create(&kek_event, SWITCH_EVENT_NOTIFY) == SWITCH_STATUS_SUCCESS) 
		{
			switch_event_add_header(kek_event, SWITCH_STACK_BOTTOM, "contact-uri","sip:iswitch@%s", kek_iswitch);
			switch_event_add_header(kek_event, SWITCH_STACK_BOTTOM, "to-uri","sip:iswitch@%s", kek_iswitch);
			switch_event_add_header(kek_event, SWITCH_STACK_BOTTOM, "from-uri","sip:mcs@%s:%s", local_ip, ip_port);

			switch_event_add_header(kek_event, SWITCH_STACK_BOTTOM, "content-type","%s","application/json");
			switch_event_add_header(kek_event, SWITCH_STACK_BOTTOM, "event-string","%s","gw "GW_VERSION);			 
			switch_event_add_header(kek_event, SWITCH_STACK_BOTTOM, "no-sub-state","%s","active");
			switch_event_add_header(kek_event, SWITCH_STACK_BOTTOM, "profile","%s",switch_core_get_domain(SWITCH_FALSE));
			switch_event_add_header(kek_event, SWITCH_STACK_BOTTOM, "extra-headers", "Type: %s","EVT");

			buffer[0] = 0;

			switch_snprintf(buffer, sizeof(buffer), "{\"Name\":\"NSB_GEN_CONF_KEK_REQ\",\"confId\":\"%s\",\"hostPublicKey\":\"48646daf156af6a\"}",
				call_id);

			if (!zstr(buffer))
				switch_event_add_body(kek_event, "%s", buffer);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "M2 Send Get KEK Event:%s\n", switch_event_get_body(kek_event));	// peter
	
			switch_event_fire(&kek_event);	
		}
	} // end of kek
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"disable kek\n");
	}
}


SWITCH_STANDARD_APP(gw_function)
{
	switch_event_t *event;
 	switch_event_header_t *hp;

	switch_channel_t *caller_channel = switch_core_session_get_channel(session);
	switch_core_session_t *peer_session = NULL;
	const char *v_campon = NULL, *v_campon_retries, *v_campon_sleep, *v_campon_timeout, *v_campon_fallback_exten = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;
	int campon_retries = 100, campon_timeout = 10, campon_sleep = 10, tmp, camping = 0, fail = 0, thread_started = 0;
	struct camping_stake stake = { 0 };
	const char *moh = NULL;
	switch_thread_t *thread = NULL;
	switch_threadattr_t *thd_attr = NULL;
	char *camp_data = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int camp_loops = 0;
	const char *var;
	char to_data[200] = {0};
	const char *callid;

	
	// calling to gw

	switch_channel_get_variables(caller_channel, &event);
	if (event) 
	{
		
		for (hp = event->headers; hp; hp = hp->next) 
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "event, name = %s, val = %s\n", hp->name, hp->value);
		}
	}
	switch_event_destroy(&event);


	if ((var = switch_channel_get_variable(caller_channel, "sip_req_user"))) 
	{
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "sip_req_user = %s\n", var);
		sprintf(to_data, "sofia/gateway/iswitch/%s", var);
	}
	else
	{
		sprintf(to_data, "sofia/gateway/iswitch/*2000086100");
	}


	callid = switch_channel_get_variable(caller_channel, "sip_call_id");

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "callid = %s\n", callid);

	send_kek_req(callid);

	if ((status = switch_ivr_originate(session, &peer_session, &cause, to_data, 0, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL)) != SWITCH_STATUS_SUCCESS) 
	{
			fail = 1;
	}

	if (fail) 
	{
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Originate Failed.  Cause: %s\n", switch_channel_cause2str(cause));

		switch_channel_set_variable(caller_channel, "originate_failed_cause", switch_channel_cause2str(cause));

		switch_channel_handle_cause(caller_channel, cause);

		return;
	} 
	else 
	{

		switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
		if (switch_true(switch_channel_get_variable(caller_channel, SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE)) ||
			switch_true(switch_channel_get_variable(peer_channel, SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE))) {
			switch_channel_set_flag(caller_channel, CF_BYPASS_MEDIA_AFTER_BRIDGE);
		}

		if (switch_channel_test_flag(caller_channel, CF_PROXY_MODE)) {
			switch_ivr_signal_bridge(session, peer_session);
		} else {
			char *a_key = (char *) switch_channel_get_variable(caller_channel, "bridge_terminate_key");
			char *b_key = (char *) switch_channel_get_variable(peer_channel, "bridge_terminate_key");
			int ok = 0;
			switch_input_callback_function_t func = NULL;

			if (a_key) {
				a_key = switch_core_session_strdup(session, a_key);
				ok++;
			}
			if (b_key) {
				b_key = switch_core_session_strdup(session, b_key);
				ok++;
			}
			if (ok) {
				func = bridge_on_dtmf;
			} else {
				a_key = NULL;
				b_key = NULL;
			}

			switch_ivr_multi_threaded_bridge(session, peer_session, func, a_key, b_key);
		}

		if (peer_session) {
			switch_core_session_rwunlock(peer_session);
		}
	}
}

static void send_oam_notify(char *code, char *filename, char *call_id)
{
	switch_event_t *oam_event;
	switch_time_t now_ts;
	char *hostname;
	char *local_ip;
	char *ip_port;
	char *oam_iswitch;
	char *oam_process_name;

	char *oam_enable;
	char *snmp_enable;

	char buffer[1024];

	// snmp
	char pathname[512];
	static int sn=0;

	// uuid
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
		
/*
<!-- OAM & SNMP config -->
<!-- X-PRE-PROCESS cmd="set" data="oam_enable=true"/ -->    <!-- default is false -->
<!-- X-PRE-PROCESS cmd="set" data="snmp_enable=true"/ -->   <!-- default is false -->
<!-- X-PRE-PROCESS cmd="set" data="oam_iswitch_ip_port=172.16.0.92:5060"/-->
<X-PRE-PROCESS cmd="set" data="oam_iswitch_ip_port=$${local_ip_v4}:5060"/>      <!-- default is local ip:5060 -->
<X-PRE-PROCESS cmd="set" data="oam_process_name=gw"/>

<!-- Recording config -->
<!-- X-PRE-PROCESS cmd="set" data="record_enable=false"/ -->   <!-- default is true -->

	*/
	hostname = switch_core_get_variable("hostname");
	local_ip = switch_core_get_variable("local_ip_v4");
	ip_port = switch_core_get_variable("internal_sip_port");
	oam_iswitch = switch_core_get_variable("oam_iswitch_ip_port");
	oam_process_name = switch_core_get_variable("oam_process_name");
	oam_enable = switch_core_get_variable("oam_enable");
	snmp_enable = switch_core_get_variable("snmp_enable");


//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"hostname = %s\n", hostname);
//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"local_ip_v4 = %s\n", local_ip);
//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"internal_sip_port = %s\n", ip_port);
//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"oam_iswitch_ip_port = %s\n", oam_iswitch);
//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"oam_process_name = %s\n", oam_process_name);

	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"oam_enable = %s\n", oam_enable);
	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"snmp_enable = %s\n", snmp_enable);

	if (zstr(oam_iswitch))
		return;

	now_ts = switch_time_ref();

	if(oam_enable && switch_true(oam_enable))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"enable oam\n");
		if(switch_event_create(&oam_event, SWITCH_EVENT_NOTIFY) == SWITCH_STATUS_SUCCESS) 
		{
			// switch_event_add_header(oam_event, SWITCH_STACK_BOTTOM, "contact-uri","%s","sip:iswitch@172.16.0.92:5060");
			// switch_event_add_header(oam_event, SWITCH_STACK_BOTTOM, "to-uri","%s","sip:iswitch@172.16.0.92:5060");
			// switch_event_add_header(oam_event, SWITCH_STACK_BOTTOM, "from-uri","%s","sip:mcs@172.16.0.92:5070");

			switch_event_add_header(oam_event, SWITCH_STACK_BOTTOM, "contact-uri","sip:iswitch@%s", oam_iswitch);
			switch_event_add_header(oam_event, SWITCH_STACK_BOTTOM, "to-uri","sip:iswitch@%s", oam_iswitch);
			switch_event_add_header(oam_event, SWITCH_STACK_BOTTOM, "from-uri","sip:mcs@%s:%s", local_ip, ip_port);

			switch_event_add_header(oam_event, SWITCH_STACK_BOTTOM, "content-type","%s","application/json");
			switch_event_add_header(oam_event, SWITCH_STACK_BOTTOM, "event-string","%s","gw "GW_VERSION);			 
			switch_event_add_header(oam_event, SWITCH_STACK_BOTTOM, "no-sub-state","%s","active");
			switch_event_add_header(oam_event, SWITCH_STACK_BOTTOM, "profile","%s",switch_core_get_domain(SWITCH_FALSE));
			switch_event_add_header(oam_event, SWITCH_STACK_BOTTOM, "extra-headers", "Type: %s","EVT");

			switch_uuid_get(&uuid);
			switch_uuid_format(uuid_str, &uuid);

			buffer[0] = 0;

			if(strcmp(code, "000") == 0)
			{
				switch_snprintf(buffer, sizeof(buffer), "{\"msg\":\"%s\",\"alarmType\":\"%s\",\"systemName\":\"%s\",\"serverIp\":\"%s\",\"msgId\":\"%s\",\"Severity\":\"%s\",\"alarmCode\":\"%s\",\"addr\":\"%s\",\"timestamp\":\"%lld\",\"Remark\":\"%s\"}",
				"gw Started",
				"gw.0",
				hostname,
				local_ip,
				uuid_str,
				"Info",
				code,
				local_ip,
				(long long int)now_ts,
				GW_VERSION
				);
				/*
				switch_snprintf(buffer, sizeof(buffer), "{\"alarmType\": \"%s\", \"alarmCode\": \"%s\", \"Severity\": \"%s\", \"Description\": \"%s\", \"Category\": \"%s\", \"Attributes\": \"%s\", \"Host\": \"%s\", \"Now\": \"%lld\", \"Name\": \"%s\"}",
					"gw.0",
					code,
					"Info",
					"gw Started",
					"System",
					"",
					local_ip,
					(long long int)now_ts,
					"OAM_ALARM_REQ"
					);
					*/
			}
			else if(strcmp(code, "099") == 0)
			{
				switch_snprintf(buffer, sizeof(buffer), "{\"msg\":\"%s\",\"alarmType\":\"%s\",\"systemName\":\"%s\",\"serverIp\":\"%s\",\"msgId\":\"%s\",\"Severity\":\"%s\",\"alarmCode\":\"%s\",\"addr\":\"%s\",\"timestamp\":\"%lld\",\"Remark\":\"%s\"}",
				"gw Stopped",
				"gw.0",
				hostname,
				local_ip,
				uuid_str,
				"Critical",
				code,
				local_ip,
				(long long int)now_ts,
				GW_VERSION
				);
			}
			else if(strcmp(code, "001") == 0)
			{
				switch_snprintf(buffer, sizeof(buffer), "{\"msg\":\"%s\",\"alarmType\":\"%s\",\"systemName\":\"%s\",\"serverIp\":\"%s\",\"msgId\":\"%s\",\"Severity\":\"%s\",\"alarmCode\":\"%s\",\"addr\":\"%s\",\"timestamp\":\"%lld\",\"Remark\":\"%s\"}",
				"Error when playing audio file",
				"gw.0",
				hostname,
				local_ip,
				uuid_str,
				"Warning",
				code,
				local_ip,
				(long long int)now_ts,
				filename
				);

			}
			else if(strcmp(code, "002") == 0)
			{
				switch_snprintf(buffer, sizeof(buffer), "{\"msg\":\"%s\",\"alarmType\":\"%s\",\"systemName\":\"%s\",\"serverIp\":\"%s\",\"msgId\":\"%s\",\"Severity\":\"%s\",\"alarmCode\":\"%s\",\"addr\":\"%s\",\"timestamp\":\"%lld\",\"Remark\":\"%s\"}",
				"File format is bad or does not exist",
				"gw.0",
				hostname,
				local_ip,
				uuid_str,
				"Warning",
				code,
				local_ip,
				(long long int)now_ts,
				filename
				);
			}

//			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Send OAM Event : %s\n", buffer);

			if (!zstr(buffer))
				switch_event_add_body(oam_event, "%s", buffer);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "M2 Send OAM Event:%s\n", switch_event_get_body(oam_event));	// peter
	
			switch_event_fire(&oam_event);	
		}
	} // end of oam
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"disable oam\n");
	}
	
	if(snmp_enable && switch_true(snmp_enable))
	{
		switch_time_exp_t tm;
		switch_time_t now = switch_micro_time_now();
		char date[64];
		FILE *fp;
		

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"enable snmp\n");

		switch_time_exp_lt(&tm, now);
		switch_snprintf(date, sizeof(date), "%0.4d%0.2d%0.2d%0.2d%0.2d%0.2d%0.3d",	
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec/1000);

		sn++;
		system("mkdir -p /mmcc/Queue/Alarm/gw");
		system("chown mmccadmin.mmccadmin /mmcc/Queue/Alarm/gw");
		sprintf(pathname, "/mmcc/Queue/Alarm/gw/gwArm_%s_%04d", date, sn);

		fp = fopen(pathname, "w");
		if(fp != NULL)
		{

			if(strcmp(code, "000") == 0)
			{
				switch_snprintf(buffer, sizeof(buffer), "ServerIP=%s&Process_Name=%s&Time=%s&Alarm_Type=%s&Alarm_Code=%s&Severity=%s&Description=%s&Remark=%s", 
					local_ip,
					oam_process_name,
					date,
					"gw.0",
					code,
					"Info",
					"gw Started",
					GW_VERSION
					);
			}
			else if(strcmp(code, "099") == 0)
			{
				switch_snprintf(buffer, sizeof(buffer), "ServerIP=%s&Process_Name=%s&Time=%s&Alarm_Type=%s&Alarm_Code=%s&Severity=%s&Description=%s&Remark=%s", 
					local_ip,
					oam_process_name,
					date,
					"gw.0",
					code,
					"Critical",
					"gw Stopped",
					GW_VERSION
					);
			}
			else if(strcmp(code, "001") == 0)
			{
				switch_snprintf(buffer, sizeof(buffer), "ServerIP=%s&Process_Name=%s&Time=%s&Alarm_Type=%s&Alarm_Code=%s&Severity=%s&Description=%s&Remark=%s", 
					local_ip,
					oam_process_name,
					date,
					"gw.0",
					code,
					"Warning",
					"Error when playing audio file",
					filename
					);
			}
			else if(strcmp(code, "002") == 0)
			{
				switch_snprintf(buffer, sizeof(buffer), "ServerIP=%s&Process_Name=%s&Time=%s&Alarm_Type=%s&Alarm_Code=%s&Severity=%s&Description=%s&Remark=%s", 
					local_ip,
					oam_process_name,
					date,
					"gw.0",
					code,
					"Warning",
					"File format is bad or does not exist",
					filename
					);
			}

			fprintf(fp, "%s\n", buffer);
			fclose(fp);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"Write SNMP msg : %s\n", buffer);
		}
	}
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"disable snmp\n");
	}
}


static void send_dtmf_notify(gw_call_leg_t * cl, const char dtmf)
{
	switch_event_t *s_event = NULL;
	switch_time_t now;

	// Send DTMF_PRESSED_NOTI
	// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "switch_event_create begin\n");

	if (switch_event_create(&s_event, SWITCH_EVENT_NOTIFY) == SWITCH_STATUS_SUCCESS) 
	{
		char *hostname;
		char *local_ip;
		char *ip_port;
		char *fax_notify_iswitch;
	
		hostname = switch_core_get_variable("hostname");
		local_ip = switch_core_get_variable("local_ip_v4");
		ip_port = switch_core_get_variable("internal_sip_port");
		fax_notify_iswitch = switch_core_get_variable("fax_notify_ip_port");
				
		// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "switch_event_create OK\n");

		/* ¼È®ÉÃö³¬
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact-uri","%s",cl->contact_uri);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "to-uri","%s",cl->from_uri);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from-uri","%s",cl->to_uri);
		*/

		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,  "contact-uri : %s\n",cl->contact_uri);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,  "to-uri : %s\n",cl->from_uri);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,  "from-uri : %s\n",cl->to_uri);


		// for test
		// switch_event_add_header(fax_notify_event, SWITCH_STACK_BOTTOM, "contact-uri","%s","sip:iswitch@172.16.0.92:5060");
		// switch_event_add_header(fax_notify_event, SWITCH_STACK_BOTTOM, "to-uri","%s","sip:iswitch@172.16.0.92:5060");
		// switch_event_add_header(fax_notify_event, SWITCH_STACK_BOTTOM, "from-uri","%s","sip:mcs@172.16.0.92:5070");
		
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact-uri","sip:iswitch@%s", fax_notify_iswitch);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "to-uri","sip:iswitch@%s", fax_notify_iswitch);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from-uri","sip:mcs@%s:%s", local_ip, ip_port);

		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,  "contact-uri : sip:iswitch@%s\n", fax_notify_iswitch);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,  "to-uri : sip:iswitch@%s\n", fax_notify_iswitch);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,  "from-uri : sip:mcs@%s:%s\n", local_ip, ip_port);




		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "content-type","%s","application/json");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "event-string","%s","gw "GW_VERSION);			 
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "no-sub-state","%s","active");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "profile","%s",switch_core_get_domain(SWITCH_FALSE));
		if(cl->x_call_id)
		{
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "call-id","%s",cl->x_call_id);
			//switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "extra-headers", "X-GW-REPLY: %s\nType: EVT",log_buf);
		}
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "extra-headers", "Type: %s","EVT");
			
		// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "add body\n");

		// for DTMF_PRESSED_NOTI
		switch_event_add_body(s_event, DTMF_PRESSED_EVT_FMT0,
			cl->call_id,
			dtmf);
		
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "Send Event:%s\n", switch_event_get_body(s_event));

		switch_event_fire(&s_event);
	}
}

static void gw_conference_execute_cmd(gw_call_leg_t* cl,int flag,const char *cmd,switch_core_session_t * session)
{

	if(GW_CL_STATUS_IS_READY(cl)){
		
		char * c1;
		switch_status_t status;
		switch_stream_handle_t stream = { 0 };
		SWITCH_STANDARD_STREAM(stream);
		
		c1 = switch_string_replace(cmd,"$member_id",cl->str_member_id);

		// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "iHMP %s EXEC Call API : [conference %s]\n",cl->call_name,c1);
		
		status = switch_api_execute("conference",c1,session,&stream);
		
		switch_snprintf(cl->error_msg,sizeof(cl->error_msg)-1,"Code:%d, %s",status,(char *)stream.data);

		if(status != SWITCH_STATUS_SUCCESS)
		{
			if(strstr(c1,"play "))
			{
				cl->play_error=1;
			}

			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_ERROR, "iHMP %s EXEC Call API : conference %s \n\tResult:%s\n",cl->call_name,c1,cl->error_msg);
			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_ERROR, "M2 %s EXEC Call API : [conference %s]\n\tResult:%s\n",cl->call_name,c1,cl->error_msg);	// peter

			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_ERROR, "M2 EXEC Call API : [conference %s]\n\tResult:%s\n", c1, cl->error_msg);	// peter
			// 2020/2/4 §ï¦¨¥H¤U¨â¦æ
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_ERROR, "M2 EXEC Call API : [conference %s]\n", c1);	// peter
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_ERROR, "\tResult:%s\n", cl->error_msg);				// peter
		}
		else{
			
			if(flag & GW_CL_STATUS_NOT)
				cl->status &= ~flag;
			else 
				cl->status |= flag;
#if 0			
			if(strstr(c1,"play ") && strstr(cl->error_msg,"not found")){
				cl->play_error=1;
			}
#endif 
			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_DEBUG, "iHMP %s EXEC Call API : conference %s \n\tResult:%s\n",cl->call_name,c1,cl->error_msg);
			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "M2 %s EXEC Call API : [conference %s]\n\tResult:%s\n",cl->call_name,c1,cl->error_msg);	// peter


			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "M2 EXEC Call API : [conference %s]\n\tResult:%s\n", c1, cl->error_msg);	// peter
			// 2020/2/4 ¤W­±³o¦æ§ï¦¨¥H¤U³o¨â¦æ
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "M2 EXEC Call API : [conference %s]\n", c1);	// peter
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "\tResult:%s\n", cl->error_msg);	// peter

			/*
			if(strstr(cl->error_msg, "Code:0, OK"))
			{
				char *ptr = strstr(cl->error_msg, "sent to conference");

				if(strlen(ptr)>6)
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "Talking %s\n", &ptr[5]);
			}
			*/
		}

		switch_safe_free(stream.data);
		switch_safe_free(c1);

	}else{
		if(cl->wait_cmds==NULL){
			switch_event_create_subclass(&cl->wait_cmds, SWITCH_EVENT_CLONE, NULL);
		}
		if(cl->wait_cmds){
			switch_event_add_header(cl->wait_cmds, SWITCH_STACK_BOTTOM, cmd, "%d",flag);
			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, "iHMP %s PUSH Call API : conference %s \n",cl->call_name,cmd);
			// switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "M2 %s PUSH Call API : conference %s \n",cl->call_name,cmd);	// peter
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "M2 PUSH Call API : conference %s \n", cmd);	// peter
		}
	}
}

static void  gw_call_execute_vcmd(gw_call_leg_t* cl,int flag, const char *fmt, ...)
{
	int ret = 0;
	char *data;
	va_list ap;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (ret == -1) {
		return ;
	}
	gw_conference_execute_cmd(cl,flag,data,NULL);
	switch_safe_free(data)
}

static void gw_retry_ev()
{
	void * pop;
	int ret;
	
	int retry_count=0;
	int p_count=0;
	int p2_count=0;
	int w_count=0;

	time_t  now = time(NULL);


	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Entry %s\n",__FUNCTION__);

	if(globals.debug >=5)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Entry %s\n",__FUNCTION__);
	
	while (switch_queue_trypop(globals.event_retry_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		switch_event_t *event = (switch_event_t *) pop;
		retry_count++;
		ret = actual_message_query_handler(event,0,NULL);
	
		if(ret == GW_RETURN_RETRY){
			const char * gw_event_retry_ts = switch_event_get_header(event,"gw_event_retry_ts");
			time_t ts = gw_event_retry_ts ? (time_t)atoll(gw_event_retry_ts) :0;


			if(gw_event_retry_ts && (now > ts ) && ( (now - ts ) > 10) ) {
				//10s 

				switch_event_destroy(&event);
				p2_count++;	
			}else { 
				
				if(gw_event_retry_ts==NULL){
					switch_event_add_header(event,SWITCH_STACK_BOTTOM,"gw_event_retry_ts","%d",(int)now);
				}

				switch_queue_push(globals.event_tmp_queue,event);
			}

		}else{ 
			p_count++;
			switch_event_destroy(&event);
		}
	}

	while (switch_queue_trypop(globals.event_tmp_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		switch_event_t *event = (switch_event_t *) pop;
		w_count++;
		switch_queue_push(globals.event_retry_queue,event);
	}	
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"M2 Process Retry Event: %d %d %d %d\n",retry_count,p_count,p2_count,w_count);

	if(globals.debug >=5)
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Leave %s\n",__FUNCTION__);
}

static void gw_call_execute_wait_cmds(gw_call_leg_t* cl)
{

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Entry %s\n",__FUNCTION__);

	if(cl->wait_cmds)
	{
		switch_event_header_t *header = NULL;
		int c=0;

		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "cl->wait_cmds is true\n");


		for (header = cl->wait_cmds->headers; header; header = header->next) {
			int flag = (int) strtol(header->value,NULL,10);
			gw_conference_execute_cmd(cl,flag,header->name,NULL);
			c++;
		}
		switch_event_destroy(&cl->wait_cmds);
		cl->wait_cmds=NULL;

		// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_INFO,"iHMP %s Process Wait Event: %d\n",cl->call_name,c);
		// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO,"M2 %s Process Wait Event: %d\n",cl->call_name,c);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO,"M2 Process Wait Event: %d\n", c);

	}
	else
	{
	//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "cl->wait_cmds is false\n");
	}


	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "calling gw_retry_ev()\n");
	gw_retry_ev();
}

static void gw_call_leg_play(gw_call_leg_t * cl,int lineno)
{
	char * prefix = cl->MySound_prefix;


	// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, "iHMP %s gw_call_leg_play line:%d \n", cl->call_name,lineno);
	// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "M2 %s gw_call_leg_play line:%d \n", cl->call_name,lineno);	// peter

	if(cl->FileList[cl->File_idx][0] == '/'){
		prefix="";
	}

	if(cl->PlayAll)
		gw_call_execute_vcmd(cl,GW_CL_STATUS_PLAY,"%d play {play-cookie=%s}%s%s async",
				cl->room_num,cl->play_cookie,prefix ,cl->FileList[cl->File_idx] );
	else
		gw_call_execute_vcmd(cl,GW_CL_STATUS_PLAY,"%d play {play-cookie=%s}%s%s $member_id async",
			cl->room_num,cl->play_cookie,prefix ,cl->FileList[cl->File_idx]);
}

static void gw_call_leg_stop_play(gw_call_leg_t * cl,int flag,int lineno)
{
	// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_DEBUG, "iHMP %s gw_call_leg_stop_play flag=%s  line:%d \n", cl->call_name, flag == GW_STOP_PLAY_ALL ? "all":"current",lineno);
	// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "M2 %s gw_call_leg_stop_play flag=%s  line:%d \n", cl->call_name, flag == GW_STOP_PLAY_ALL ? "all":"current",lineno);	// peter
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "M2 gw_call_leg_stop_play flag=%s\n", flag == GW_STOP_PLAY_ALL ? "all":"current");	// peter

	if(GW_CL_STATUS_IS_PLAYING(cl)){
		if(cl->PlayAll)		// ³o­Ó§PÂ_¬O¤£¬O¦³°ÝÃD, ¦]¬°¥»¨Ó¥i¯à¬O 0, ·í¦¬¨ì·sªº PLAY_REQ «á³o cl->PlayAll ¬O·sªº¼Æ­È¤F, ¦ý¬O·sªº¼Æ­È cl->PlayAll=1  ¥Nªí­n¹ï conf romm ¤º¥þ³¡ªº¤H©ñ­µ, ¨º»ò stop all ¤]¨S¿ùªü, ¦ý¬O·|µo¥Í "°±¤î¤£¤F"
			gw_call_execute_vcmd(cl,GW_CL_STATUS_PLAY|GW_CL_STATUS_NOT,"%d stop all",cl->room_num);
		else
			gw_call_execute_vcmd(cl,GW_CL_STATUS_PLAY|GW_CL_STATUS_NOT,"%d stop all $member_id ",cl->room_num );

		// peter
		if(strstr(cl->error_msg, "Stopped 0 files"))	// 2019/2/27 retry
		{
			gw_call_execute_vcmd(cl,GW_CL_STATUS_PLAY|GW_CL_STATUS_NOT,"%d stop all",cl->room_num);
		}

		if(flag == GW_STOP_PLAY_ALL)	// ¦MÀI, ­Y¬O¥Î¤@­Ó play ¤¤Â_¤W¤@­Ó play «h³o¸Ì·|µo¥Í°ÝÃD
		{
			cl->RepeatPlay=0;				//  (·sªº play cl->RepeatPlay ·|³Q§ï¬° 0)
			cl->File_idx = cl->nFileList;	//  (·sªº play cl->File_idx ·|³Q§ï¬° 1 ©Î§ó°ª)
			cl->play_cookie[0]=0;
		}
	}
}

static void gw_call_leg_start_timer(gw_call_leg_t * cl)
{


#ifdef MII90
	if(!GW_CL_STATUS_IS(cl,GW_CL_STATUS_TIMER) && (cl->InterDigitTimeOut >0 || cl->InitialTimeOut>0 )){
#else
	if(!GW_CL_STATUS_IS(cl,GW_CL_STATUS_TIMER) && (cl->InterDigitTimeOut || cl->InitialTimeOut)){
#endif 		
		// disable process InitialTimeOut
		cl->InterDigit_ts = cl->dtmf_Initial_ts = switch_time_ref();
		
		switch_mutex_lock(globals.callegs_mutex);				
		switch_core_hash_delete(globals.callegs_timeout_hash,cl->call_id);
		switch_core_hash_insert(globals.callegs_timeout_hash,cl->call_id,cl);		
		//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_DEBUG, 
		/*
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, 
			"M2 %s Add Timer InterDigitTimeOut=%lld InitialTimeOut=%lld  \n", cl->call_name,(long long int)cl->InterDigit_ts , (long long int)cl->dtmf_Initial_ts );		
		*/
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, 
			"M2 Add Timer InterDigitTimeOut=%lld InitialTimeOut=%lld  \n", (long long int)cl->InterDigit_ts , (long long int)cl->dtmf_Initial_ts );
		GW_CL_STATUS_SET(cl,GW_CL_STATUS_TIMER);
		switch_mutex_unlock(globals.callegs_mutex);
	}
}

static const char * gw_play_code_to_str(int c)
{

	switch(c){
	case GW_PLAY_CODE_DONE:return "done";
	case GW_PLAY_CODE_TERM_KEY:return "term_key";
	case GW_PLAY_CODE_MAX_KEY:return "max_key";
	case GW_PLAY_CODE_INPUT_TIMEOUT:return "input_timeout";
	case GW_PLAY_CODE_INPUT_DIGIT_TIMEOUT:return "input_digit_timeout";
	case GW_PLAY_CODE_KEY_MISS_MASK:return "miss_mask";
	case GW_PLAY_CODE_PLAY_ERROR:return "error";
	case GW_STOP_PLAY_CURRENT:return "current";
	case GW_STOP_PLAY_ALL:return "all";
		
	}
	return "unknown";
}

static void show_log(gw_call_leg_t* cl, const char *format, ...)
{
	va_list ap;
	int rc;
	char *data=NULL;

	va_start(ap, format);
	rc = switch_vasprintf(&data, format, ap);
	va_end(ap);

	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "Send Event:%s\n", data);

	if(data != NULL)
		free(data);
}

static void copy_char(char *from, char *to)
{
	int len;
	int i;

	len = strlen(from);
	for(i=0; i<len; i++)
	{
		to[i] = 'X';
		to[i+1] = 0;
	}
}

static void gw_report_dtmf_event_(gw_call_leg_t* cl,int flag,int disable_timeout)
{
	char input[256]={0};
	char userinput[256]={0};
	char h_input[256]={0};
	char h_userinput[256]={0};
	char log_buf[512];
	switch_event_t *s_event = NULL;
	int eof=0;
	switch_time_t now;

	// for peter spec case 
	// user request UsingDtmf == false and BargeIn == true 
	//  return play done message 

	if(cl->UsingDtmf==2){
		if(flag==GW_PLAY_CODE_MAX_KEY)
			flag = GW_PLAY_CODE_DONE;
	}

	if(flag==GW_PLAY_CODE_INPUT_TIMEOUT || flag==GW_PLAY_CODE_INPUT_DIGIT_TIMEOUT){
		// è¾“å…¥è¶…æ—¶
		// FIX: MII-54  MII-53

		// ?‰é”®è¶…æ—¶
		//InterDigitTimeOut 
		
		//sprintf(input,"%d",cl->InitialTimeOut_ReturnCode);
		// FIXBUG: MII-53 
		// å½“æ??®è??¶å¹¶ä¸”æ?å¤§æ??®é•¿åº¦å??€å°æ??®é•¿åº¦ä?ä¸€?´ï?å¹¶ä??¶åˆ°?„DTMF?¿åº¦å¤§ä?ç­‰ä??€å°æ??®é•¿åº¦ç??¶å€?		//   ä¸è??žé?è¯¯ï?
		if(cl->MaxKey != cl->MinKey && cl->dtmf_idx >=cl->MinKey){
			strcpy(userinput,cl->dtmf_buf);
			strcpy(input,cl->dtmf_buf);
		}else{
			strcpy(userinput,cl->dtmf_buf);
			sprintf(input,"%d",flag==GW_PLAY_CODE_INPUT_TIMEOUT ? cl->InitialTimeOut_ReturnCode : cl->InterDigitTimeOut_ReturnCode );

#ifdef MII53
			// MII53 ¿é¤J¿ù»~­n¶Ç¦^ -2 ¦Ó¤£¬O -1, ©Ò¥H±j¨î¥[¤J¥H¤UªºÂà´«
			if(cl->dtmf_idx >0 && cl->dtmf_idx < cl->MinKey)
			{
				// ¤£¨¬½X, ¶Ç¦^ 
				// sprintf(input,"%d",cl->DigitMask_ReturnCode);	// 2020/5/18 ¶Ç¦^ DigitMask_ReturnCode ¤£¹ï, ·í DigitMask_ReturnCode == InterDigitTimeOut_ReturnCode== -2 ®É¬Ý¤£¥X°ÝÃD
																	// ·í InterDigitTimeOut_ReturnCode = -1 ¦ÓDigitMask_ReturnCode =-2 ®É´N·|µo²{°ÝÃD¤F

				// ¤£¨¬½X, ¶Ç¦^ InterDigitTimeOut_ReturnCode			   2020/5/18
				// sprintf(input,"%d",cl->InterDigitTimeOut_ReturnCode);	// 2020/5/18 ·s¼W, 2020/5/20 mark

				sprintf(input,"%d",cl->DigitMask_ReturnCode);	// 2020/5/20, flow ¬Û®e©Ê¦Ò¶q, ±N¿ù´N¿ù, ¤£¨¬½XÁÙ¬O¶Ç¦^ DigitMask_ReturnCode (-2)
			}
			else if (cl->dtmf_idx == 0)			// 2020/5/26 ±j¨î­×¥¿, ­Y¨S¿é¤J«h¶Ç¦^ InterDigitTimeOut_ReturnCode
			{
				sprintf(input,"%d",cl->InterDigitTimeOut_ReturnCode);	
			}
#endif
		
		}

	}else if( flag==GW_PLAY_CODE_TERM_KEY || flag==GW_PLAY_CODE_MAX_KEY ){
		
		strcpy(userinput,cl->dtmf_buf);
		if(flag==GW_PLAY_CODE_TERM_KEY)
			strcat(userinput,cl->last_input);
		//FIXBUGï¼šMII-53 
		//å¦‚æ??‘çŽ°?¶åˆ°ç»“æ??‰é”®ï¼Œä??¯æ??®é•¿åº¦å?äºŽæ?å°ç??‰é”®?¿åº¦??		// è¿”å??™è¯¯
#ifndef MII102		
		// å¦‚æ??¶åˆ°GW_PLAY_CODE_TERM_KEY å°±ç??Ÿï?ä¸å??¤æ–­MinKey 
		if(flag==GW_PLAY_CODE_TERM_KEY && cl->dtmf_idx >0 && cl->dtmf_idx < cl->MinKey)
			sprintf(input,"%d",cl->DigitMask_ReturnCode);
		else 
#endif
		if(flag == GW_PLAY_CODE_MAX_KEY){
			//copy maxkey len to input
			strncpy(input,cl->dtmf_buf,cl->MaxKey);
			input[cl->MaxKey]=0x0;
		}
		else 
			strcpy(input,cl->dtmf_buf);

	}else if(flag==GW_PLAY_CODE_DONE){
		strcpy(userinput,cl->dtmf_buf);
		strcpy(input,"");
		eof=1;
	
	}else if(flag==GW_PLAY_CODE_KEY_MISS_MASK){
		strcpy(userinput,cl->dtmf_buf);
		strcat(userinput,cl->last_input);		
		sprintf(input,"%d",cl->DigitMask_ReturnCode);

	}else if(flag == GW_PLAY_CODE_PLAY_ERROR){
		strcpy(userinput,cl->dtmf_buf);
		sprintf(input,"%d",-400);	// ³o¸Ì©ñ -400 ¬O ¬°¤FÁ×§K±¼ ScriptEdit ¤Î IMMR ªº bug,
		// input[0] = 0;			// ¤£¯à¶Ç¦^ªÅ¥Õ, §_«h·|²Å¦X $INPUTDTMF==0 
	}
	
	if(disable_timeout){
		if(cl->dtmf_Initial_ts){
			//disable timer 
			switch_mutex_lock(globals.callegs_mutex);				
			switch_core_hash_delete(globals.callegs_timeout_hash,cl->call_id);
			switch_mutex_unlock(globals.callegs_mutex);
			GW_CL_STATUS_CLS(cl,GW_CL_STATUS_TIMER);
			cl->dtmf_Initial_ts=0;
		}
	}
	/*  PROP-176 ¯Ê­µÀÉ®É­Y­è¦n¬O¦b¤T¤è«h·|°õ¦æ¨ì³o¸Ìªº undeaf, bug
	gw_call_leg_stop_play(cl,GW_STOP_PLAY_ALL,__LINE__);

	if(GW_CL_STATUS_IS(cl,GW_CL_STATUS_DEAF))
		gw_call_execute_vcmd(cl,GW_CL_STATUS_NOT|GW_CL_STATUS_DEAF,"%d undeaf $member_id ",cl->room_num);	
	*/

	if(flag != GW_PLAY_CODE_PLAY_ERROR)	// this line add at 2020/6/9
	{
		gw_call_leg_stop_play(cl,GW_STOP_PLAY_ALL,__LINE__);

		if(GW_CL_STATUS_IS(cl,GW_CL_STATUS_DEAF))
			gw_call_execute_vcmd(cl,GW_CL_STATUS_NOT|GW_CL_STATUS_DEAF,"%d undeaf $member_id ",cl->room_num);	
	}

	// Send IVR_PLAY_PROMPT_EVT
	if ( cl->ReturnEvt && switch_event_create(&s_event, SWITCH_EVENT_NOTIFY) == SWITCH_STATUS_SUCCESS) {
		switch_snprintf(log_buf,sizeof(log_buf),"IVR_PLAY_PROMPT_EVT reason=%s error=%d eof=%d inputlen=%d INPUTDTMF=%s lastInput=%s UserInput=%s",
			gw_play_code_to_str(flag),cl->play_error,eof,strlen(input),input,cl->last_input,userinput);
		
		// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, "iHMP %s Report DTMF Event:%s\n",cl->call_name,log_buf);
		// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "M2 %s Report DTMF Event:%s\n",cl->call_name,log_buf);	// peter
		//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "M2 Report DTMF Event:%s\n", log_buf);	// peter 2019/8/22


	//	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "contact-uri = %s\n", cl->contact_uri);	// peter
	//	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "to-uri = %s\n", cl->from_uri);	// peter
	//	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "from-uri = %s\n", cl->to_uri);	// peter
	//	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "content-type = %s\n", "application/json");	// peter
	//	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "event-string = %s %s\n", "gw", GW_VERSION);	// peter
	//	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "no-sub-state = %s\n", "active");	// peter
	//	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "profile = %s\n", switch_core_get_domain(SWITCH_FALSE));	// peter

		
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "contact-uri","%s",cl->contact_uri);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "to-uri","%s",cl->from_uri);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "from-uri","%s",cl->to_uri);
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "content-type","%s","application/json");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "event-string","%s","gw "GW_VERSION);			 
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "no-sub-state","%s","active");
		switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "profile","%s",switch_core_get_domain(SWITCH_FALSE));
		if(cl->x_call_id){
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "call-id","%s",cl->x_call_id);
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "extra-headers", "X-GW-REPLY: %s\nType: EVT",log_buf);
		}else{
			switch_event_add_header(s_event, SWITCH_STACK_BOTTOM, "extra-headers", "Type: %s","EVT");
		}
		
		// "\"SessionID\":\"%s\","
		// "\"INPUTDTMF\":\"%s\","
		// "\"USERINPUT\":\"%s%s\","
		// "\"EOF\":\"%s\","
		// "\"TERMKEY\":\"%s\","
		// "\"AUDIOERROR\":\"%s\"}"; 
		
		if(cl->Security)
		{
			switch_event_add_body(s_event,IVR_PLAY_PROMPT_EVT_FMT2,
				cl->call_id,
				input,
				userinput,"",
				eof?"true":"false",
				flag==GW_PLAY_CODE_TERM_KEY?cl->last_input:"",
				cl->play_error?"true":"false",
				cl->play_error?cl->error_msg:""
				);
		
		//	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,	"iHMP %s Send Event:%s\n",cl->call_name,switch_event_get_body(s_event));	// peter
			copy_char(input, h_input);
			copy_char(userinput, h_userinput);

			show_log(cl, IVR_PLAY_PROMPT_EVT_FMT2,
				cl->call_id,
				h_input,
				h_userinput,"",
				eof?"true":"false",
				flag==GW_PLAY_CODE_TERM_KEY?cl->last_input:"",
				cl->play_error?"true":"false",
				cl->play_error?cl->error_msg:""
				);
		}
		else if(cl->ASR)
		{
			switch_event_add_body(s_event,IVR_PLAY_PROMPT_EVT_ASRFMT,
				cl->call_id,
				input,
				userinput,"",
				eof?"true":"false",
				flag==GW_PLAY_CODE_TERM_KEY?cl->last_input:"",
				cl->play_error?"true":"false",
				cl->ASR?"true":"false",
				cl->ASRStatus,
				cl->ASRText,
				cl->ASRResult
				);

			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE,	"iHMP %s Send Event:%s\n",cl->call_name,switch_event_get_body(s_event));
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,	"Send Event:%s\n", switch_event_get_body(s_event));	// peter
		}
		else
		{
			switch_event_add_body(s_event,IVR_PLAY_PROMPT_EVT_FMT1,
				cl->call_id,
				input,
				userinput,"",
				eof?"true":"false",
				flag==GW_PLAY_CODE_TERM_KEY?cl->last_input:"",
				cl->play_error?"true":"false",
				cl->play_error?cl->error_msg:""
				);
		
			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE,	"iHMP %s Send Event:%s\n",cl->call_name,switch_event_get_body(s_event));
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,	"Send Event:%s\n", switch_event_get_body(s_event));	// peter
		}
		switch_event_fire(&s_event);	
		
#ifdef MII55
		/*
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "cl->call_name = %s\n", cl->call_name);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "cl->from_uri = %s\n", cl->from_uri);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "cl->to_uri = %s\n", cl->to_uri);
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "cl->contact_uri = %s\n", cl->contact_uri);
		*/

		if(globals.ha_argc >0)
			now = switch_time_ref();
		
		for(int i=0;i<globals.ha_argc;i++)
		{
			//if(NULL==strstr(cl->from_uri,globals.ha_argv[i]) && NULL==strstr(globals.ha_argv[i],cl->from_uri))
			if(strstr(globals.ha_argv[i], cl->from_uri) == NULL)	// other HA server
			{
				if(globals.ha_time1[i] >0 && globals.ha_time2[i] >0 && ((now-globals.ha_time2[i]) < (globals.ha_time2[i]-globals.ha_time1[i])*3) && (now-globals.ha_time2[i]) < 10050000)	// 10's
					{
					switch_event_t *ha_event;
					
					if(switch_event_create(&ha_event, SWITCH_EVENT_NOTIFY) == SWITCH_STATUS_SUCCESS) 
					{
						
						// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, "iHMP %s Send HA Event:%s\n",cl->call_name,globals.ha_argv[i]);
						// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "M2 %s Send HA Event:%s\n",cl->call_name,globals.ha_argv[i]);	// peter
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "M2 Send HA Event:%s\n", globals.ha_argv[i]);	// peter

						switch_event_add_header(ha_event, SWITCH_STACK_BOTTOM, "contact-uri","%s",globals.ha_argv[i]);
						switch_event_add_header(ha_event, SWITCH_STACK_BOTTOM, "to-uri","%s",globals.ha_argv[i]);
						switch_event_add_header(ha_event, SWITCH_STACK_BOTTOM, "from-uri","%s",cl->to_uri);
						switch_event_add_header(ha_event, SWITCH_STACK_BOTTOM, "content-type","%s","application/json");
						switch_event_add_header(ha_event, SWITCH_STACK_BOTTOM, "event-string","%s","gw "GW_VERSION);			 
						switch_event_add_header(ha_event, SWITCH_STACK_BOTTOM, "no-sub-state","%s","active");
						switch_event_add_header(ha_event, SWITCH_STACK_BOTTOM, "profile","%s",switch_core_get_domain(SWITCH_FALSE));
						if(cl->x_call_id){
							switch_event_add_header(ha_event, SWITCH_STACK_BOTTOM, "call-id","%s",cl->x_call_id);
							switch_event_add_header(ha_event, SWITCH_STACK_BOTTOM, "extra-headers", "X-GW-REPLY: %s\nType: EVT",log_buf);
						}else{
							switch_event_add_header(ha_event, SWITCH_STACK_BOTTOM, "extra-headers", "Type: %s","EVT");
						}

						if(cl->Security)
						{
							switch_event_add_body(ha_event,IVR_PLAY_PROMPT_EVT_FMT2,
							cl->call_id,
							input,
							userinput,"",
							eof?"true":"false",
							flag==GW_PLAY_CODE_TERM_KEY?cl->last_input:"",
							cl->play_error?"true":"false",
							cl->play_error?cl->error_msg:""
							);
						}
						else if(cl->ASR)
						{
							switch_event_add_body(ha_event,IVR_PLAY_PROMPT_EVT_FMT1,
							cl->call_id,
							input,
							userinput,"",
							eof?"true":"false",
							flag==GW_PLAY_CODE_TERM_KEY?cl->last_input:"",
							cl->play_error?"true":"false",
							cl->play_error?cl->error_msg:"",
							cl->ASR?"true":"false",
							cl->ASRStatus,
							cl->ASRText,
							cl->ASRResult
							);
						}
						else
						{
							switch_event_add_body(ha_event,IVR_PLAY_PROMPT_EVT_FMT1,
							cl->call_id,
							input,
							userinput,"",
							eof?"true":"false",
							flag==GW_PLAY_CODE_TERM_KEY?cl->last_input:"",
							cl->play_error?"true":"false",
							cl->play_error?cl->error_msg:""
							);
						}
						
						switch_event_fire(&ha_event);	
					}
				}
			}
		}
#endif 		
	}

	// 2022/7/8
	if(cl->ASR)
	{
		switch_core_session_t *rsession = NULL;
		rsession = cl->session_uuid ? switch_core_session_locate(cl->session_uuid) : NULL;
		switch_ivr_stop_detect_speech(rsession);
		switch_core_session_rwunlock(rsession);	
	}

	// disable dtmf 
	cl->UsingDtmf=0;
	cl->ReturnEvt=0;
	// clear dtmf buffer
	cl->dtmf_idx=0;
	cl->dtmf_buf[0]=0;	
}

static void gw_report_dtmf_event(gw_call_leg_t* cl,int flag,int disable_timeout,int lineno)
{	
	// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,"M2 %s Fire DTMF Event:%d line:%d\n",cl->call_name,flag,lineno);
	// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,"M2 Fire DTMF Event:%d\n", flag);
	gw_report_dtmf_event_(cl,flag,disable_timeout);
}

void  gw_timeout_run()
{
	switch_hash_index_t *hi = NULL;
	
	switch_event_t *event = NULL;
	switch_event_header_t *header = NULL;

			
	switch_time_t now = switch_time_ref();

	// Number 9

	switch_event_create_subclass(&event, SWITCH_EVENT_CLONE, NULL);
	switch_assert(event);
	
	switch_mutex_lock(globals.callegs_mutex);
	for ((hi = switch_core_hash_first_iter( globals.callegs_timeout_hash, hi));hi;switch_core_hash_next(&hi)) {
		void *val = NULL;
		const void *key;
		switch_ssize_t keylen;
		gw_call_leg_t* cl;
		
		switch_core_hash_this(hi, &key, &keylen, &val);
		cl = (gw_call_leg_t *) val;

		if(cl->UsingDtmf)
		{
			if(cl->InterDigitTimeOut  
#ifdef MII90
				>0 
#endif 
				&&  (now - cl->InterDigit_ts > cl->InterDigitTimeOut)
#ifdef MII149
				&& cl->dtmf_idx >0		// ²Ä¤@­Ó«öÁä¬Ý InitialTimeOut ¦Ó¤£¬Ý InterDigitTimeOut
#endif 				
				){
				//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, 
				/*
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, 
					"M2 %s  Fire Timer InterDigitTimeOut=%lld offset=%lld  \n", cl->call_name,(long long int)cl->InterDigit_ts , (long long int)(now - cl->InterDigit_ts ));				
					*/
								
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, 
					"M2 Fire Timer InterDigitTimeOut=%lld offset=%lld  \n", (long long int)cl->InterDigit_ts , (long long int)(now - cl->InterDigit_ts ));				

				gw_report_dtmf_event(cl,GW_PLAY_CODE_INPUT_DIGIT_TIMEOUT,0,__LINE__);

				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "delete", (const char *) key);
				GW_CL_STATUS_CLS(cl,GW_CL_STATUS_TIMER);

			}else if(cl->InitialTimeOut 
#ifdef MII90
				>0 
#endif 			
			&&  (now - cl->dtmf_Initial_ts > cl->InitialTimeOut ) 
#ifdef MII149
				&& cl->dtmf_idx ==0		// ²Ä¤@­Ó«öÁä¬Ý InitialTimeOut ¦Ó¤£¬Ý InterDigitTimeOut
#endif			
			){

				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, 
				/*
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, 
					"M2 %s  Fire Timer InitialTimeOut=%lld offset=%lld  \n", cl->call_name,(long long int) cl->InitialTimeOut ,(long long int)( now - cl->dtmf_Initial_ts ));
					*/
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, 
					"M2 Fire Timer InitialTimeOut=%lld offset=%lld  \n", (long long int) cl->InitialTimeOut ,(long long int)( now - cl->dtmf_Initial_ts ));

				// gw_report_dtmf_event(cl,GW_PLAY_CODE_INPUT_TIMEOUT,0,__LINE__);  // ÂÂª©¨S¦³ InitialTimeOut
				gw_report_dtmf_event(cl,GW_PLAY_CODE_INPUT_DIGIT_TIMEOUT,0,__LINE__);	// 2020/5/20 ¬°¤F»PÂÂª©¬Û®e, ¨S«öÁä·|¶Ç¦^ InterDigitTimeOut_ReturnCode
	
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "delete", (const char *) key);
				GW_CL_STATUS_CLS(cl,GW_CL_STATUS_TIMER);
			}

		}else{
			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "GW_CL_STATUS_TIMER, delete \n");	

			GW_CL_STATUS_CLS(cl,GW_CL_STATUS_TIMER);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "delete", (const char *) key);
		}		
	}

	switch_safe_free(hi);

	/* now delete them */
	for (header = event->headers; header; header = header->next) {
		switch_core_hash_delete(globals.callegs_timeout_hash, header->value);
	}
	switch_event_destroy(&event);

	switch_mutex_unlock(globals.callegs_mutex);	
}

void *SWITCH_THREAD_FUNC gw_event_thread_run(switch_thread_t *thread, void *obj)
{
	void *pop;
	switch_time_t last=0;
	int ret;

	// Number 6
	while (globals.running == 1) 
	{
		if (switch_queue_trypop(globals.event_queue, &pop) == SWITCH_STATUS_SUCCESS)	// triggered switch_queue_push()
		{
			switch_event_t *event = (switch_event_t *) pop;

			if (!pop) {
				break;
			}
			
			ret = actual_message_query_handler(event,0,NULL);

			if(ret == GW_RETURN_RETRY){

				switch_queue_push(globals.event_retry_queue,event);

			}else{ 
				switch_event_destroy(&event);
			}
		}else {
			
			switch_time_t now = switch_time_ref();
			
			if(now - last > 100){
				gw_timeout_run();
				last=now; 
			}

			//sleep 20ms
			switch_yield(20000);

		}
	}

	while (switch_queue_trypop(globals.event_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		switch_event_t *event = (switch_event_t *) pop;
		switch_event_destroy(&event);
	}
	
	while (switch_queue_trypop(globals.event_retry_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		switch_event_t *event = (switch_event_t *) pop;
		switch_event_destroy(&event);
	}

	switch_mutex_lock(globals.mutex);
	globals.num_threads--;
	switch_mutex_unlock(globals.mutex);		

	return NULL;
}

void gw_event_thread_start(void)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	int done = 0;

	// Number 5
	switch_mutex_lock(globals.mutex);
	if (globals.num_threads==0) {
		globals.num_threads++;
	} else {
		done = 1;
	}
	switch_mutex_unlock(globals.mutex);

	if (done) {
		return;
	}

	switch_threadattr_create(&thd_attr, globals.pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_threadattr_priority_set(thd_attr, SWITCH_PRI_IMPORTANT);
	switch_thread_create(&thread, thd_attr, gw_event_thread_run, NULL, globals.pool);
}

static void gw_event_dump(switch_event_t *event)
{
	char * ebuf=NULL;

	switch_event_serialize_json(event,&ebuf);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"%s\n",ebuf);
	switch_safe_free(ebuf);
}


void gw_event_handler(switch_event_t *event)
{
	switch_event_t *cloned_event;

	// Number 4
	// have event 1

	if(globals.debug){
		// char * ebuf=NULL;
		// switch_event_serialize_json(event,&ebuf);
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"%s\n",ebuf);
		// switch_safe_free(ebuf);
	}

	if(globals.running){
		switch_event_dup(&cloned_event, event);
		switch_assert(cloned_event);
		switch_queue_push(globals.event_queue, cloned_event);		// trigger switch_queue_trypop()

		if (globals.num_threads==0) {
			gw_event_thread_start();
		}
	}
}

void check_file(const char *pathfile)
{
	struct stat file_info;
	unsigned long old_len=-1;
	int count=0;
	int error_count=0;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "check file: %s\n", pathfile);
	while(1)
	{
		if (stat(pathfile, &file_info) == 0) 
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "File size: %lld bytes\n", (long long)file_info.st_size);
			if(old_len == file_info.st_size)
			{
				count ++;
				if(count >=3)
					break;
			}
			else
			{
				old_len = file_info.st_size;
				count=0;
			}
		} 
		else
		{
			error_count++;
			if(error_count >= 100)
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Stat Error.\n");
				break;
			}
		}

		// switch_yield(100000);
		switch_yield(200000);
	}
}

static int actual_message_query_handler(switch_event_t *event,int argc,void * argv[])
{
	int ret;
	int status=GW_RETURN_OK;

	// Number 7
	// have event 2

	for(int i=0;i<globals.num_eventprocs;i++)
	{
		int flag=0;
		
		if(flag==0 && globals.eventprocs[i].e_event_id != event->event_id)
		{
			flag++;
		}
		if(flag==0 && ( globals.eventprocs[i].e_subclass_name && event->subclass_name 
				&& strcmp(globals.eventprocs[i].e_subclass_name,event->subclass_name) ))
		{
			flag++;
		}
		if(flag==0 && globals.eventprocs[i].key && 
			strcmp(switch_event_get_header(event, globals.eventprocs[i].key),globals.eventprocs[i].value) )
		{
			flag++;
		}

		if(flag==0)
		{
			ret = globals.eventprocs[i].func(event,argc,argv);	// triggering

			if(ret == GW_RETURN_RETRY )
			{
				status=GW_RETURN_RETRY;
			}
		}
	}

	return status;
}

static int gw_ev_hangup_handler(switch_event_t *event,int argc ,void * argv[])
{
	const char * gw_call_id=NULL;
//	char path1[512];
//	char path2[512];
//	char mess[1100];
//	recording_t *ptr;
//	recording_t *next;

	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "coming to gw_ev_hangup_handler()\n");	// peter
	// gw_event_dump(event);	// peter

	if(NULL!=( gw_call_id = switch_event_get_header(event, "variable_gw_call_id")))
	{
		gw_call_leg_t * cl = gw_find_callleg (gw_call_id);

		if(cl)
		{
			gw_conference_room_t * room = gw_find_room(cl->room_num);

			if(room && room->members){
				switch_event_del_header(room->members,cl->call_id);
			}

			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, "iHMP %s Free Call ID\n",gw_call_id);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "M2 Free Call-ID:%s\n",gw_call_id);

			if(cl->active_audio_stream)
			{
				switch_stream_handle_t mystream = { 0 };
				char command[100];
				char mess[1000];

				SWITCH_STANDARD_STREAM(mystream);

				cl->active_audio_stream = 1;	

				sprintf(command, "uuid_audio_stream");
				sprintf(mess, "%s stop", cl->session_uuid);	

				switch_api_execute(command, mess, NULL, &mystream);
				if (mystream.data) 
				{
				}

				switch_safe_free(mystream.data);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "uuid_audio_stream %s\n", mess);
			}

			
			gw_rm_callleg(gw_call_id);
			switch_core_destroy_memory_pool(&cl->pool);

			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "gw_rm_callleg:%s\n",gw_call_id);

		}else{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Not Found iHMP Call ID %s in Hash \n",gw_call_id);
		}
		
	}

	return GW_RETURN_OK;
}

static int gw_ev_notify_in_handler(switch_event_t *event,int argc ,void * argv[])
{
	int ret = GW_RETURN_OK;
	const char * content_type;
	const char * sip_method;
	int i;

	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "coming to gw_ev_notify_in_handler()\n");	// peter
	// gw_event_dump(event);	// peter

	// no Unique-ID

	// ping msg
	// {"Event-Name":"NOTIFY_IN","Core-UUID":"684505b1-be73-4f5e-bd2b-79f527cd79cd","FreeSWITCH-Hostname":"iswitch02","FreeSWITCH-Switchname":"iswitch02","FreeSWITCH-IPv4":"172.16.0.92","FreeSWITCH-IPv6":"::1","Event-Date-Local":"2019-03-22 11:03:58","Event-Date-GMT":"Fri, 22 Mar 2019 03:03:58 GMT","Event-Date-Timestamp":"1553223838275097","Event-Calling-File":"sofia.c","Event-Calling-Function":"sofia_handle_sip_i_options","Event-Calling-Line-Number":"10770","Event-Sequence":"1092","sip-method":"OPTIONS","contact-host":"172.16.0.92:5060","contact-uri":"sip:(null)@172.16.0.92:5060","from-uri":"sip:ping@172.16.0.92:5070","to-uri":"sip:ping@172.16.0.92:5070","call-id":"ae4bbc2186b804879a2710f321120753@172.16.0.92"}

	//  play msg
	// {"Event-Name":"NOTIFY_IN","Core-UUID":"684505b1-be73-4f5e-bd2b-79f527cd79cd","FreeSWITCH-Hostname":"iswitch02","FreeSWITCH-Switchname":"iswitch02","FreeSWITCH-IPv4":"172.16.0.92","FreeSWITCH-IPv6":"::1","Event-Date-Local":"2019-03-22 11:04:00","Event-Date-GMT":"Fri, 22 Mar 2019 03:04:00 GMT","Event-Date-Timestamp":"1553223840435099","Event-Calling-File":"sofia.c","Event-Calling-Function":"sofia_handle_sip_i_notify","Event-Calling-Line-Number":"557","Event-Sequence":"1095","sip-method":"NOTIFY","pl_data":"{\"system\":\"true\",\"request\":{\"InitialTimeOut_ReturnCode\":\"-1\",\"SessionID\":\"9bc84af27e08c7cdd200c22be32c4871@172.16.0.92\",\"MinKey\":1,\"Domain\":\"system_default\",\"ClearBuffer\":true,\"Barge-in\":false,\"InterDigitTimeOut_ReturnCode\":\"-2\",\"UUID\":\"\",\"TermKey\":\"#\",\"RepeatPlay\":true,\"Name\":\"IVR_PLAY_PROMPT_REQ\",\"InitialTimeOut\":500,\"DigitMask_ReturnCode\":\"-3\",\"Using-Dtmf\":false,\"DigitMask\":\"1234567890*#\",\"InterDigitTimeOut\":1500,\"ReturnEvt\":\"true\",\"FileList\":[\"HoldMusic.wav\"],\"Language Type\":\"TW\",\"MaxKey\":12}}","content-type":"application/json","from-uri":"sip:iswitch@172.16.0.92:5060","to-uri":"sip:mcs@172.16.0.92:5070","call-id":"475142276313099269153762428680","Content-Length":"502","_body":"{\"system\":\"true\",\"request\":{\"InitialTimeOut_ReturnCode\":\"-1\",\"SessionID\":\"9bc84af27e08c7cdd200c22be32c4871@172.16.0.92\",\"MinKey\":1,\"Domain\":\"system_default\",\"ClearBuffer\":true,\"Barge-in\":false,\"InterDigitTimeOut_ReturnCode\":\"-2\",\"UUID\":\"\",\"TermKey\":\"#\",\"RepeatPlay\":true,\"Name\":\"IVR_PLAY_PROMPT_REQ\",\"InitialTimeOut\":500,\"DigitMask_ReturnCode\":\"-3\",\"Using-Dtmf\":false,\"DigitMask\":\"1234567890*#\",\"InterDigitTimeOut\":1500,\"ReturnEvt\":\"true\",\"FileList\":[\"HoldMusic.wav\"],\"Language Type\":\"TW\",\"MaxKey\":12}}"}

	/*
	// peter 2019/2/25
	switch_core_session_t *session =NULL;
	const char * uuid;

	uuid=switch_event_get_header(event, "Unique-ID");	// uuid is null,
	session=uuid?switch_core_session_locate(uuid):NULL;
	// No,  can't get uuid & session,  // peter
	*/

	// Number 8
	// have event 3
	sip_method = switch_event_get_header(event, "sip-method");
	content_type = switch_event_get_header(event, "content-type");

//	plog("content-type = %s\n", content_type);
//	plog("pl_data    = %s\n", switch_event_get_header(event, "pl_data"));		// body
//	plog("contact-uri = %s\n", switch_event_get_header(event, "contact-uri"));	// Contact
//	plog("from-uri = %s\n", switch_event_get_header(event, "from-uri"));		// From
//	plog("to-uri = %s\n", switch_event_get_header(event, "to-uri"));			// To
//	plog("call-id = %s\n", switch_event_get_header(event, "call-id"));			// Call-ID

	if(strcmp(sip_method, "OPTIONS")==0)
	{
		const char * contact_host;
		const char * from_uri;

		contact_host = switch_event_get_header(event, "contact-host");
		from_uri = switch_event_get_header(event, "from-uri");

//		plog("contact-host = %s\n", contact_host);
//		plog("from-uri = %s\n", from_uri);

		if(strstr(from_uri, "ping"))
		{
			//add host to argv
			for(i=0; i<globals.ha_argc; i++)
			{
				if(strstr(globals.ha_argv[i], contact_host))
					break;
			}
			if( i == globals.ha_argc)
			{
				char mess[200];

				sprintf(mess, "sip:iswitch@%s", contact_host);
				globals.ha_argv[globals.ha_argc++] = strdup(mess);
			}
			// 

			// check iswitch server is online
			if(globals.ha_argc)
			{
				int flag = 0;
				switch_time_t now = switch_time_ref();
	
				for(int i=0;i<globals.ha_argc;i++)
				{
					if(strstr(globals.ha_argv[i], contact_host))
					{
						if(globals.ha_time1[i] == 0)
							globals.ha_time1[i] = now;
						else if(globals.ha_time2[i] == 0)
							globals.ha_time2[i] = now;
						else
						{
							globals.ha_time1[i] = globals.ha_time2[i];
							globals.ha_time2[i] = now;
						}

						/*
						if(globals.ha_time1[i] > 0 && globals.ha_time2[i] > 0)
							plog("Matched HA-%d : %s, ha_time[%d] = %ld\n", i, globals.ha_argv[i], i, globals.ha_time2[i] - globals.ha_time1[i]);
						else
							plog("Matched HA-%d : %s\n", i, globals.ha_argv[i]);
						*/

						flag = 1;
					}
					/*
					else
						plog("Not matched HA-%d : %s\n", i, globals.ha_argv[i]);
						*/
				}

				/*
				if(flag == 0)
				{
					for(int i=0;i<globals.ha_argc;i++)
					{
						plog("HA-%d:%s, but contact_host is %s\n", i, globals.ha_argv[i], contact_host);
					}
				}
				*/

			} // end of globals.ha_argc
		} // end of strstr
	}
	else
	{
		/* peter can't get UniqueID & uuid
		if(strcmp(sip_method, "NOTIFY")==0)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "is NOTIFY\n");	
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "UniqueID = %s\n", switch_event_get_header(event, "UniqueID"));	
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "uuid = %s\n", switch_event_get_header(event, "uuid"));	
		}
		*/
	}

	if(content_type && strcmp(content_type,"application/json")==0)
	{
		const char * body = switch_event_get_body(event);	// equal switch_event_get_header(event, "pl_data"));
		cJSON * parsed = body ? cJSON_Parse(body) :NULL;

		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "body = %s\n", body?body:"");	// 2024/1/31 for test 
#ifdef MII98		
		const char * gw_event_retry_ts = switch_event_get_header(event,"gw_event_retry_ts");
		if( gw_event_retry_ts)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "RETRY NOTIFY : %s\n",body?body:"");
			// plog("RETRY NOTIFY :  %s\n",body?body:"");
		}
#endif 	

		if(parsed)
		{
//			if(cJSON_GetObjectCstr(parsed,"system") && 
//				0== strcmp("true",cJSON_GetObjectCstr(parsed,"system")))
			{

				cJSON * req= cJSON_GetObjectItem(parsed,"request");
				if(req)
				{
					switch_event_t *new_event = NULL;
					const char * name=cJSON_GetObjectCstr(req,"Name");
					const char * call_id;

					if(strcasecmp(name, "IVR_PLAY_PROMPT_REQ") == 0)
						call_id=cJSON_GetObjectCstr(req, "SessionID");
					else if(strcasecmp(name, "MERGE_CALL_REQ") == 0)
						call_id=cJSON_GetObjectCstr(req, "call_id2");
					else if(strcasecmp(name, "RECORD_CALL_REQ") == 0)
						call_id=cJSON_GetObjectCstr(req, "CallID");
					else if(strcasecmp(name, "RECORD_PARTY_REQ") == 0)
						call_id=cJSON_GetObjectCstr(req, "CallID");
					else if(strcasecmp(name, "RECORD_STOP_REQ") == 0)
						call_id=cJSON_GetObjectCstr(req, "CallID");
					else if(strcasecmp(name, "DETECT_DTMF_START_REQ") == 0)
						call_id=cJSON_GetObjectCstr(req, "SessionID");
					else
						call_id=cJSON_GetObjectCstr(req, "call_id1");	// ¥u¨ú call-id1 ªº©R¥O¥þ³¡¦b³o, ¦p UNMERGE_CALL_REQ

					if(call_id)
					{
						gw_call_leg_t * cl=gw_find_callleg(call_id);

						if(cl)	// ¨S§PÂ_ª½±µ¥Î cl->session_sn ·|·í¾÷
							switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "RECV NOTIFY :  %s\n",body?body:"");
						else
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "RECV NOTIFY :  %s\n",body?body:"");
					}
					else
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "RECV NOTIFY :  %s\n",body?body:"");


					// Dispatch IVR_PLAY_PROMPT_REQ / IVR_MERGE_CALL / ....
					if(name && switch_event_create_subclass(&new_event, SWITCH_EVENT_CUSTOM,SWITCH_EVENT_SUBCLASS_GW) == SWITCH_STATUS_SUCCESS)
					{
						void * argv[3];
						argv[0]=event;	// ev
						argv[1]=req;	// cJSON

						switch_event_add_header_string(new_event, SWITCH_STACK_BOTTOM, "Action",name);		// name = IVR_PLAY_PROMPT_REQ or ......
						argv[2] = switch_event_get_header(event, "contact-uri");

						ret = actual_message_query_handler(new_event,2,argv);	// triggering gw_ev_play_prompt_handler, gw_ev_merge_call_handler,....
						
						// if (ret == GW_RETURN_RETRY)..... // peter


						switch_event_destroy(&new_event);
					}
				}
			} 

			cJSON_Delete(parsed);
			return ret;
		}
		else
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "RECV NOTIFY :  %s\n",body?body:"");

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "M2 Parse JSON FAILED : %s\n",body);	
		}
	}
	else
	{
		//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "iHMP Notify Content-Type Is NOT application/json \n");	
	}

	return ret;
}

int cJSON_GetObjectInt(const cJSON *object, const char *string,int dval)
{
	cJSON *cj = cJSON_GetObjectItem(object, string);

	if(cj){
		if(cj->type==cJSON_String ){
			if(isdigit(cj->valuestring[0]) || cj->valuestring[0]=='-')
				dval =atoi(cj->valuestring);
			else if(strcasecmp(cj->valuestring,"true")==0 || strcasecmp(cj->valuestring,"on")==0){
				dval =1;
			} else if(strcasecmp(cj->valuestring,"false")==0 || strcasecmp(cj->valuestring,"off")==0){
				dval = 0; 
			}
		}
		else if(cj->type == cJSON_Number)
			dval = cj->valueint;
		else if(cj->type == cJSON_True)
			dval = 1;
		else if(cj->type == cJSON_False)
			dval= 0;
	}

	return dval;
}

const char * cJSON_GetObjectStrcpy(const cJSON *object, const char *string, char * d,int len,const char * dval)
{
	const char * buf = cJSON_GetObjectCstr(object, string);

	switch_snprintf(d,len,"%s",buf?buf:dval);

	return d ;
}

static int gw_ev_unmute_call_call_handler(switch_event_t *event,int argc ,void * argv[])
{
	cJSON * req = (void*)argv[1];
	// const char * call_id=cJSON_GetObjectCstr(req,"call_id");
	const char * call_id=cJSON_GetObjectCstr(req,"call_id1");

	if(call_id){
		gw_call_leg_t * cl=gw_find_callleg(call_id);
		if(cl){

			if(GW_CL_STATUS_IS_MUTE(cl))
			{
				gw_call_execute_vcmd(cl,GW_CL_STATUS_NOT|GW_CL_STATUS_MUTE,"%d unmute $member_id ",cl->room_num);
				// GW_CL_STATUS_CLS_MUTE(cl);	// peter 2019/3/5, 2019/8/29 ¸Ñ°£, ¦]¬°¤w¸g¦b gw_call_execute_vcmd ¤º°µ CLS/SET ¤F, ¥H¤U¸I¨ì¦¹®×¨Ò¥u mark, ¤£¦A»¡©ú
			}
		}
	}else
	{
		// send_does_not_exist_evt_notify(call_id);	// 2023/8/2
		return GW_RETURN_RETRY;
	}

	return GW_RETURN_OK;	
}

static int gw_ev_mute_call_call_handler(switch_event_t *event,int argc ,void * argv[])
{
	cJSON * req = (void*)argv[1];
	// const char * call_id=cJSON_GetObjectCstr(req,"call_id");
	const char * call_id=cJSON_GetObjectCstr(req,"call_id1");

	if(call_id){
		gw_call_leg_t * cl=gw_find_callleg(call_id);
		if(cl){

			if(!GW_CL_STATUS_IS_MUTE(cl))
			{
				// gw_call_execute_vcmd(cl,GW_CL_STATUS_NOT|GW_CL_STATUS_MUTE,"%d mute $member_id ",cl->room_num);	// 2020/1/14 ³o¦æ¦³°ÝÃD, µLªk³]©w¬° MUTE ª¬ºA
				gw_call_execute_vcmd(cl, GW_CL_STATUS_MUTE, "%d mute $member_id ", cl->room_num);
				// GW_CL_STATUS_SET_MUTE(cl);	// peter 2019/3/5	
			}
		}
	}
	else
	{
		// send_does_not_exist_evt_notify(call_id);	// 2023/8/2
		return GW_RETURN_RETRY;
	}

	return GW_RETURN_OK;	
}


static int gw_ev_adjust_volume_call_handler(switch_event_t *event,int argc ,void * argv[])
{
//	switch_event_t * ev=(switch_event_t*)argv[0];
	cJSON * req = (void*)argv[1];
//	const char * param = argc > 2 ? (const char *)argv[2] : NULL ;
	const char * call_id=cJSON_GetObjectCstr(req,"call_id");
	int volume =cJSON_GetObjectInt(req,"volume",0);

	if(call_id){
		gw_call_leg_t * cl=gw_find_callleg(call_id);
		if(cl){
			// if(volume !=0 && cl->volume_out_flag==0){
			// 	char *p;
				
			// 	gw_call_execute_vcmd(cl,0,"%d volume_out %s up",cl->room_num,cl->str_member_id);
				
			// 	if((p = strstr("=",cl->error_msg))!=NULL ){
			// 		while(*p && !isdigit(*p))
			// 			p++;
			// 		if(*p){
			// 			cl->volume_out = atoi(p)-1;
			// 			cl->volume_out_flag=1;
			// 		}
			// 	}
			// 	gw_call_execute_vcmd(cl,0,"%d volume_out %s down",cl->room_num,cl->str_member_id);
			// }

			// if(cl->volume_out_flag)
			{
				gw_call_execute_vcmd(cl,0,"%d volume_out %s %d",cl->room_num,cl->str_member_id,cl->volume_out + volume);
			}
		}
	}else{
		return GW_RETURN_RETRY;
	}

	return GW_RETURN_OK;
}

static int gw_ev_record_call_handler(switch_event_t *event,int argc ,void * argv[])
{
	int ret= GW_RETURN_OK;
	cJSON * req = (void*)argv[1];
	const char * call_id=cJSON_GetObjectCstr(req,"CallID");
	char mess[512];
	switch_core_session_t *rsession = NULL;
	uint32_t limit = 0;
	char path[512];

		
	if(call_id)
	{
		gw_call_leg_t * cl=NULL;
	//	int members;

		cl = gw_find_callleg(call_id);	// find call leg from hash-table by call_id

		if(cl)
		{
			gw_recording_t * rec;

			// if(!GW_CL_STATUS_IS_READY(cl))	// peter 2019/4/10	add, 2023/8/1 mark
			if(cl->channel_active == 0)	// 2023/8/1 ³Q unmerge call ®É, ·|µu¼È³Q³]¬° not ready, ©Ò¥H§ï¥Î·sªº°Ñ¼Æ¨Ó¨ú¥N
			{
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_ERROR, "M2 RECORD_CALL FAILED: IS NOT READAY %s\n", cl->call_name);

				return GW_RETURN_RETRY;
			}

			rec = (gw_recording_t*)switch_core_alloc(globals.pool, sizeof(gw_recording_t));

			rec->Rec_Mode = GW_REC_SYNC;	// 2019/12/6

			cJSON_GetObjectStrcpy(req,"RecordPath",rec->RecordPath,sizeof(rec->RecordPath),"");
			cJSON_GetObjectStrcpy(req,"RecordFile",rec->RecordFile,sizeof(rec->RecordFile),"");
			cJSON_GetObjectStrcpy(req,"Type",rec->Rec_Type,sizeof(rec->Rec_Type),"wav");

			rec->Stereo			= cJSON_GetObjectInt(req,"Stereo",0);
			rec->OnlyCallLeg	= cJSON_GetObjectInt(req,"OnlyCallLeg",0);
			rec->Duration		= cJSON_GetObjectInt(req,"Duration",0);

			rec->isBlockRecording = 0;	// peter 2019/12/17

			if (switch_directory_exists(rec->RecordPath, globals.pool) != SWITCH_STATUS_SUCCESS) 
			{
				// dir_status = switch_dir_make(cl->RecordPath, SWITCH_FPROT_OS_DEFAULT, globals.pool);
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "switch_dir_make = %d\n", dir_status);
				sprintf(mess, "mkdir -p %s", rec->RecordPath);
				system(mess);
			}

			if (!(rsession = switch_core_session_locate(cl->session_uuid))) 
			{
			//	stream->write_function(stream, "-ERR Cannot locate session!\n");
		
			}
			else	// have 'session'
			{
				switch_channel_t *channel = switch_core_session_get_channel(rsession);

				if(rec->Stereo == 1)
				{
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "Stereo is true\n");

					switch_channel_set_variable(channel, "RECORD_STEREO", "true");
				}
				else
					switch_channel_set_variable(channel, "RECORD_STEREO", "false");


				// for test
				// sprintf(cl->record_current->OnlyCallLeg, "true");
				// sprintf(cl->record_current->Duration, "10");

				limit = 0;

				if(rec->OnlyCallLeg == 1)
				{
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "OnlyCallLeg is true\n");

					switch_channel_set_variable(channel, "RECORD_READ_ONLY", "true");
					limit = rec->Duration;
				}
				else
					switch_channel_set_variable(channel, "RECORD_READ_ONLY", "false");


				switch_channel_set_variable(channel, "RECORD_SILENCE_THRESHOLD", "0");	// 2019/12/6

				if(!strcmp(rec->Rec_Type, "wav") || !strcmp(rec->Rec_Type, "mp3"))
					sprintf(path, "%stemp%s.wav", rec->RecordPath, rec->RecordFile);
				else
					sprintf(path, "%stemp%s.%s", rec->RecordPath, rec->RecordFile, rec->Rec_Type);	// 2022/10/7

				sprintf(rec->TempPathFile, "%s", path);

				// insert hash
				switch_mutex_lock(globals.recording_mutex);		
				switch_core_hash_insert(globals.recording_hash, rec->TempPathFile, rec);
				switch_mutex_unlock(globals.recording_mutex);
				//

				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "Recording %s\n", path);

				if (switch_ivr_record_session(rsession, path, limit, NULL) != SWITCH_STATUS_SUCCESS)
				{
					//	stream->write_function(stream, "-ERR Cannot record session!\n");
				}
				else 
				{
					//	stream->write_function(stream, "+OK Success\n");
				}

				switch_core_session_rwunlock(rsession);		// ¦³«Å§i switch_core_session_t ¨Ã¨ú±o­È®ÉÂ÷¶}¬Ò»Ý©I¥s¦¹¨ç¼ÆÄÀ©ñ
			}
		}
		else	// 2019/8/2 add by peter
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not Find cl\n");

			// send_does_not_exist_evt_notify(call_id);	// 2023/8/2
			return GW_RETURN_RETRY;
			//return GW_RETURN_OK;		// 2023/8/2
		}
	}
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not Find Call Leg : %s \n", call_id);
		ret = GW_RETURN_RETRY;
	}

	return ret;	
}

static int gw_ev_record_stop_handler(switch_event_t *event,int argc ,void * argv[])
{
	int ret= GW_RETURN_OK;
	cJSON * req = (void*)argv[1];
	const char * call_id=cJSON_GetObjectCstr(req,"CallID");
	switch_core_session_t *rsession = NULL;
//	recording_t *ptr;
//	recording_t *next;
	char path1[512];
	char RecordFile[150];
	char RecordPath[100];
	char Type[16];

	if(call_id)
	{
		gw_call_leg_t * cl=NULL;

		cl = gw_find_callleg(call_id);	// find call leg from hash-table by call_id

		if(cl)
		{
			rsession = cl->session_uuid ? switch_core_session_locate(cl->session_uuid) : NULL;
			if(rsession == NULL)
			{
				//	stream->write_function(stream, "-ERR Cannot locate session!\n");

			}
			else	// have 'session'
			{
				memset(RecordFile, 0, sizeof(RecordFile));
				cJSON_GetObjectStrcpy(req,"RecordFile",RecordFile,sizeof(RecordFile),"");

				memset(RecordPath, 0, sizeof(RecordPath));
				cJSON_GetObjectStrcpy(req,"RecordPath",RecordPath,sizeof(RecordPath),"");

				memset(Type, 0, sizeof(Type));
				cJSON_GetObjectStrcpy(req,"Type",Type,sizeof(Type),"");	// 2022/10/7

				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "cl->record_head = %p.\n", (void *)ptr);

				if(strlen(RecordFile)>0)
				{
					// sprintf(path1, "%stemp%s.wav", RecordPath, RecordFile); 2022/10/7
					if(!strcmp(Type, "wav") || !strcmp(Type, "mp3"))
						sprintf(path1, "%stemp%s.wav", RecordPath, RecordFile);
					else
						sprintf(path1, "%stemp%s.%s", RecordPath, RecordFile, Type);

					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "Stop Recording : %s\n", path1);

					if (switch_ivr_stop_record_session(rsession, path1) != SWITCH_STATUS_SUCCESS) 
					{
						//	stream->write_function(stream, "-ERR Cannot record session!\n");
					} 
					else 
					{
						//	stream->write_function(stream, "+OK Success\n");
					}
				}
				switch_core_session_rwunlock(rsession);
			} // end of else
		} // end of if(cl)
		else
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not Find cl : %s\n", call_id);
	}
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not Find Call Leg : %s \n", call_id);

		// send_does_not_exist_evt_notify(call_id);	// 2023/8/2
		ret = GW_RETURN_RETRY;
	}

	return ret;	
}


// static int gw_party_record_stop(gw_call_leg_t * cl, char *pRecordPath, char *pRecordFile)
static int gw_party_record_stop(gw_call_leg_t * cl)	// 2022/10/7
{
	int ret= GW_RETURN_OK;
	switch_core_session_t *rsession = NULL;
	char path1[512];

	if(!cl)
		return ret;

	if(!GW_CL_STATUS_IS(cl,GW_CL_STATUS_BLOCK_RECORD))
		return ret;

	// if(strlen(pRecordPath) ==0 && strlen(pRecordFile) == 0)
	if(strlen(cl->pRecordPath) ==0 && strlen(cl->pRecordFile) == 0)
		return ret;

	if(strlen(cl->pRecordType) ==0)	// 2022/10/7
		return ret;
	

	rsession = cl->session_uuid ? switch_core_session_locate(cl->session_uuid) : NULL;
	if(rsession == NULL)
	{
		//	stream->write_function(stream, "-ERR Cannot locate session!\n");
	}
	else	// have 'session'
	{
		if(strlen(cl->pRecordFile)>0)
		{
			// sprintf(path1, "%stemp%s.wav", pRecordPath, pRecordFile); 2022/10/7
			if(!strcmp(cl->pRecordType, "wav") || !strcmp(cl->pRecordType, "mp3"))
				sprintf(path1, "%stemp%s.wav", cl->pRecordPath, cl->pRecordFile);
			else
				sprintf(path1, "%stemp%s.%s", cl->pRecordPath, cl->pRecordFile, cl->pRecordType);

			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "Stop Recording : %s\n", path1);

			if (switch_ivr_stop_record_session(rsession, path1) != SWITCH_STATUS_SUCCESS) 
			{
				//	stream->write_function(stream, "-ERR Cannot record session!\n");
			} 
			else 
			{
				//	stream->write_function(stream, "+OK Success\n");
			}
		}
		switch_core_session_rwunlock(rsession);
	}

	return ret;	
}

static int gw_ev_record_party_handler(switch_event_t *event,int argc ,void * argv[])	// block recording
{
	int ret= GW_RETURN_OK;
	cJSON * req = (void*)argv[1];
	const char * call_id=cJSON_GetObjectCstr(req,"CallID");
	char mess[512];
	switch_core_session_t *rsession = NULL;
	uint32_t limit = 0;
	char path[512];

		
	if(call_id)
	{
		gw_call_leg_t * cl=NULL;
	//	int members;

		cl = gw_find_callleg(call_id);	// find call leg from hash-table by call_id

		if(cl)
		{
			gw_recording_t * rec;

			if(!GW_CL_STATUS_IS_READY(cl))	// peter 2019/4/10
			{
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_ERROR, "M2 RECORD_CALL FAILED: IS NOT READAY %s\n", cl->call_name);

				return GW_RETURN_RETRY;
			}

			gw_call_leg_stop_play(cl, GW_STOP_PLAY_ALL, __LINE__);		//  ¦]¬O block recording, ©Ò¥H¥ý°±¤î©ñ­µ

			// gw_party_record_stop(cl, cl->pRecordPath, cl->pRecordFile);	//  ¦]¬O block recording, ©Ò¥H¥ý°±¤î¿ý­µ
			gw_party_record_stop(cl);	// 2022/10/7

			/*
			RECV NOTIFY :  {"request":{
			"TerminationKey":"",
			"Type":"wav",
			"RecordFile":"20191203093359538-MS-iSwitch-002-1024-47009-1_M71",
			"CallID":"3358f70a5333d746MTVlOWVjYzQyNDljNGIwNmVlMTgxNWEzYzlmMDBiY2Y.",
			"Duration":0,
			"RecordPath":"/Data/RecordingFiles/M71/20191203/",
			"SilenceTerminationThreshold":0,
			"Stereo":true,
			"OnlyCallLeg":true,
			"Name":"RECORD_PARTY_REQ"}}
			*/
			rec = (gw_recording_t*)switch_core_alloc(globals.pool, sizeof(gw_recording_t));

			rec->Rec_Mode = GW_REC_BLOCK;	// 2019/12/6

			cJSON_GetObjectStrcpy(req,"RecordPath",rec->RecordPath,sizeof(rec->RecordPath),"");
			cJSON_GetObjectStrcpy(req,"RecordFile",rec->RecordFile,sizeof(rec->RecordFile),"");
			cJSON_GetObjectStrcpy(req,"Type",rec->Rec_Type,sizeof(rec->Rec_Type),"wav");
			cJSON_GetObjectStrcpy(req,"TerminationKey",rec->TermKey,sizeof(rec->TermKey),"");

			sprintf(cl->RecTermKey, "%s", rec->TermKey);

			// stop record ®É»Ý­n RecordPath & RecordFile, °O¿ý«ü¼Ð§Y¥i
			cl->pRecordPath = rec->RecordPath;
			cl->pRecordFile = rec->RecordFile;
			cl->pRecordType = rec->Rec_Type;


			rec->Stereo			= cJSON_GetObjectInt(req,"Stereo",0);
			rec->OnlyCallLeg	= cJSON_GetObjectInt(req,"OnlyCallLeg",0);
			rec->Duration		= cJSON_GetObjectInt(req,"Duration",0);
			rec->Silence		= cJSON_GetObjectInt(req,"SilenceTerminationThreshold",0);

			rec->isBlockRecording = 1;	// peter 2019/12/17

			if (switch_directory_exists(rec->RecordPath, globals.pool) != SWITCH_STATUS_SUCCESS) 
			{
				// dir_status = switch_dir_make(cl->RecordPath, SWITCH_FPROT_OS_DEFAULT, globals.pool);
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "switch_dir_make = %d\n", dir_status);
				sprintf(mess, "mkdir -p %s", rec->RecordPath);
				system(mess);
			}

			if (!(rsession = switch_core_session_locate(cl->session_uuid))) 
			{
			//	stream->write_function(stream, "-ERR Cannot locate session!\n");
		
			}
			else	// have 'session'
			{
				switch_channel_t *channel = switch_core_session_get_channel(rsession);

				if(rec->Stereo == 1)
				{
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "Stereo is true\n");

					switch_channel_set_variable(channel, "RECORD_STEREO", "true");
				}
				else
					switch_channel_set_variable(channel, "RECORD_STEREO", "false");

				// for test
				// sprintf(cl->record_current->OnlyCallLeg, "true");
				// sprintf(cl->record_current->Duration, "10");

				limit = 0;

				if(rec->OnlyCallLeg == 1)
				{
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "OnlyCallLeg is true\n");

					switch_channel_set_variable(channel, "RECORD_READ_ONLY", "true");
					limit = rec->Duration;
				}
				else
					switch_channel_set_variable(channel, "RECORD_READ_ONLY", "false");

				switch_channel_set_variable_printf(channel, "RECORD_SILENCE_THRESHOLD", "200");
				switch_channel_set_variable_printf(channel, "record-silence-hits", "3");


				//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, "set Silence = %d\n", rec->Silence);

				sprintf(path, "%stemp%s.wav", rec->RecordPath, rec->RecordFile);
				sprintf(rec->TempPathFile, "%s", path);

				// insert hash
				switch_mutex_lock(globals.recording_mutex);		
				switch_core_hash_insert(globals.recording_hash, rec->TempPathFile, rec);
				switch_mutex_unlock(globals.recording_mutex);
				//

				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "Recording %s\n", path);

				if (switch_ivr_record_session(rsession, path, limit, NULL) != SWITCH_STATUS_SUCCESS)
				{
					//	stream->write_function(stream, "-ERR Cannot record session!\n");
				}
				else 
				{
					//	stream->write_function(stream, "+OK Success\n");
				}

				GW_CL_STATUS_SET(cl, GW_CL_STATUS_BLOCK_RECORD);

				switch_core_session_rwunlock(rsession);
			}
		}
		else	// 2019/8/2 add by peter
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not Find cl\n");
			ret = GW_RETURN_RETRY;
		}
	}
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not Find Call Leg : %s \n", call_id);
		ret = GW_RETURN_RETRY;
	}

	return ret;	
}

static int gw_ev_detect_dtmf_handler(switch_event_t *event,int argc ,void * argv[])		// peter 2022/6/10
{
	switch_event_t * ev=(switch_event_t*)argv[0];
	cJSON * req = (void*)argv[1];
	const char * call_id=cJSON_GetObjectCstr(req,"SessionID");

	if(call_id){
		gw_call_leg_t * cl=gw_find_callleg(call_id);
		if(cl){

			cl->send_dtmf_event = 1;

			if(ev)
			{
				// add at 2022/6/20
				cl->contact_uri = switch_core_strdup(cl->pool,switch_event_get_header(ev, "contact-uri"));
				cl->from_uri = switch_core_strdup(cl->pool,switch_event_get_header(ev, "from-uri"));
				cl->to_uri = switch_core_strdup(cl->pool,switch_event_get_header(ev, "to-uri"));
				if(zstr( cl->contact_uri) ){
					cl->contact_uri=switch_core_strdup(cl->pool,cl->from_uri);
				}

				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "cl->contact_uri = %s\n", cl->contact_uri);
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "cl->from_uri = %s\n", cl->from_uri);
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "cl->to_uri = %s\n", cl->to_uri);
			}
		}
	}else{
		return GW_RETURN_RETRY;
	}

	return GW_RETURN_OK;	
}


static int gw_ev_merge_call_handler(switch_event_t *event,int argc ,void * argv[])
{
	//int i;
	int ret= GW_RETURN_OK;
	switch_event_t * ev=(switch_event_t*)argv[0];
	cJSON * req = (void*)argv[1];
	
	const char * param = argc > 2 ? (const char *)argv[2] : NULL ;

	const char * call_id1=cJSON_GetObjectCstr(req,"call_id1");
	const char * call_id2=cJSON_GetObjectCstr(req,"call_id2");

	if(call_id1 && call_id2)
	{
		gw_call_leg_t * cl1=NULL;
		gw_call_leg_t * cl2=NULL;

		cl1 = gw_find_callleg(call_id1);	// find call leg from hash-table by call_id
		cl2 = gw_find_callleg(call_id2);

		// ¥|ºØ±¡ªp»Ý­n¦Ò¼{
		// ¥H¤U¨âªÌ cl1->room_num != cl2->room_num
		// ¤w¦³ coach call, ·sªº call ³Q merge ¶i¨Ó, ¥Ø«e¬Ý°_¨Ó¦³°ÝÃD, ·sªºcall Å¥±o¨ì coach ªÌÁn­µ
		// ¤w¦³ monitor call, ·sªº call ³Q merge ¶i¨Ó, ¥Ø«e¬Ý°_¨Ó¦³°ÝÃD, ¥þ³¡¥i¤¬¬Û³q¸Ü

		// ¥H¤U¨âªÌ cl1->room_num == cl2->room_num
		// ¤w¦³ coach call, Âà merge
		// ¤w¦³ monitor call, Âà merge

		if(cl1 && cl2 && cl1->wait_reinvite==0 && cl2->wait_reinvite==0)
		{

			if(!GW_CL_STATUS_IS_READY(cl1) || !GW_CL_STATUS_IS_READY(cl2))	// peter 2019/4/10
			{
				if(!GW_CL_STATUS_IS_READY(cl1))
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_ERROR, "M2 MERGE_CALL FAILED: IS NOT READAY %s\n", cl1->call_name);

				if(!GW_CL_STATUS_IS_READY(cl2))
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl2->session_sn), SWITCH_LOG_ERROR, "M2 MERGE_CALL FAILED: IS NOT READAY %s\n", cl2->call_name);

				return GW_RETURN_RETRY;
			}

			// 2022/1/20 ·s¼W·í merge_call_req ¨ä call_id1 == call_id2 ®ÉÂ÷¶}·|Ä³«Ç
			if(param == NULL && !strcmp(call_id1, call_id2))	// ¤£¯à¥Î call_id1 == call_id2
			{
				/*  2022/1/21 ¥H¤U³o¬q´ú¸Õok, ¦ý¬O·í²Ä¤T¤è²¾¥X room «á, ³Ñ¤Uªº¨â¤è­Y¦³¤@¤è±¾½u«h·|±¾½u
				*/
				// ¦¹¤èªk¥u·|¥Î¦b WebRTC ¤T¤è«á«ö«O¯d, ÁÙ·|¨ú¦^«O¯dªº, ©Ò¥H¤£­n²¾¥X¬°¦n, ¥u­n mute & deaf §Y¥i

				// 2022/1/21
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_INFO, "WebRTC hold.\n");

				if(!GW_CL_STATUS_IS(cl1,GW_CL_STATUS_DEAF))
					gw_call_execute_vcmd(cl1,GW_CL_STATUS_DEAF,"%d deaf $member_id ",cl1->room_num);
					
				if(!GW_CL_STATUS_IS_MUTE(cl1))
					gw_call_execute_vcmd(cl1,GW_CL_STATUS_MUTE,"%d mute $member_id ",cl1->room_num);	
				
				return ret;
			}

			if(param==NULL)	// 2020/3/9, is merge call, not monitor & coach
			{
				gw_call_leg_stop_play(cl1,GW_STOP_PLAY_ALL,__LINE__);
				gw_call_leg_stop_play(cl2,GW_STOP_PLAY_ALL,__LINE__);
			}

			if(ev)
			{
				if(switch_event_get_header(ev,"X-GW-REQ"))
				{
					cl1->x_call_id = switch_core_strdup(cl1->pool,switch_event_get_header(ev, "call-id"));
					cl2->x_call_id = switch_core_strdup(cl2->pool,switch_event_get_header(ev, "call-id"));
				}
				
				cl1->contact_uri = switch_core_strdup(cl1->pool,switch_event_get_header(ev, "contact-uri"));
				cl1->from_uri = switch_core_strdup(cl1->pool,switch_event_get_header(ev, "from-uri"));
				cl1->to_uri = switch_core_strdup(cl1->pool,switch_event_get_header(ev, "to-uri"));

				if(zstr(cl1->contact_uri))
					cl1->contact_uri = switch_core_strdup(cl1->pool,cl1->from_uri);

				cl2->contact_uri = switch_core_strdup(cl2->pool,switch_event_get_header(ev, "contact-uri"));
				cl2->from_uri = switch_core_strdup(cl2->pool,switch_event_get_header(ev, "from-uri"));
				cl2->to_uri = switch_core_strdup(cl2->pool,switch_event_get_header(ev, "to-uri"));

				if(zstr(cl2->contact_uri))
					cl2->contact_uri = switch_core_strdup(cl2->pool,cl2->from_uri);
			}

			if( cl1->room_num != cl2->room_num)	 // merge new party
			{
				gw_call_leg_t * cll1=cl1;
				gw_call_leg_t * cll2=cl2;
				char *sip_to_user = NULL;
				char *sip_caller = NULL;
				switch_stream_handle_t mystream = { 0 };
				char command[100];
				char mess[1000];

			
				SWITCH_STANDARD_STREAM(mystream);

				sprintf(command, "uuid_audio_stream");
				sprintf(mess, "%s start ws://192.168.1.17:3001/asr mono 16k \"uuid\":\"%s\",\"Type\":\"twoWay\",\"ANI\":\"%s\",\"DNIS\":\"%s\",\"party\":\"A\"", 
					cl1->session_uuid, cl1->session_uuid, cl1->sip_ANI, cl1->sip_DNIS);

				switch_api_execute(command, mess, NULL, &mystream);
				if (mystream.data) 
				{
				}

				switch_safe_free(mystream.data);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "call uuid_audio_stream %s\n", mess);
				
				//

				switch_yield(20000);

				// Called
				
				SWITCH_STANDARD_STREAM(mystream);

				sprintf(command, "uuid_audio_stream");
				sprintf(mess, "%s start ws://192.168.1.17:3001/asr mono 16k \"uuid\":\"%s\",\"Type\":\"twoWay\",\"ANI\":\"%s\",\"DNIS\":\"%s\",\"party\":\"B\"", 
					cl2->session_uuid, cl1->session_uuid, cl1->sip_ANI, cl1->sip_DNIS);

				switch_api_execute(command, mess, NULL, &mystream);
				if (mystream.data) 
				{
				}

				switch_safe_free(mystream.data);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "call uuid_audio_stream %s\n", mess);
				//

				
				if(!(cl1->dtmf_type == SWITCH_DTMF_RTP && cl2->dtmf_type == SWITCH_DTMF_RTP))
				{
					switch_core_session_t *session =NULL;
					switch_status_t status;

					if(cl1->generate_dtmf_tone ==0)
					{
						session=cl1->session_uuid?switch_core_session_locate(cl1->session_uuid):NULL;
						if(session)
						{
							status = switch_ivr_inband_dtmf_generate_session(session, SWITCH_FALSE);		// write
							cl1->generate_dtmf_tone = 1;
							switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_DEBUG, "inband_dtmf_generate, status = %d\n", status);
							switch_core_session_rwunlock(session);
						}
					}

					if(cl2->generate_dtmf_tone ==0)
					{
						session=cl2->session_uuid?switch_core_session_locate(cl2->session_uuid):NULL;
						if(session)
						{
							status = switch_ivr_inband_dtmf_generate_session(session, SWITCH_FALSE);		// write
							cl2->generate_dtmf_tone = 1;
							switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl2->session_sn), SWITCH_LOG_DEBUG, "inband_dtmf_generate, status = %d\n", status);
							switch_core_session_rwunlock(session);
						}
					}
				}
				
				if(gw_get_room_member_count( cl2->room_num ) > gw_get_room_member_count( cl1->room_num ) )
				{
					// ?¹æ®??¸ª?¿é—´?„ç”¨?·å?å°±å??¥åˆ°??¸ª?¿é—´
					cll1=cl2;
					cll2=cl1;
				}

				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_uuid), SWITCH_LOG_NOTICE,
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_DEBUG, "M2 callid:%s transfer %s \n",cll1->call_name,cll2->call_name);

				if(GW_CL_STATUS_IS_READY(cll1) && GW_CL_STATUS_IS_READY(cll2))
				{

					gw_call_execute_vcmd(cll2,GW_CL_STATUS_MERGE,"%d transfer %d $member_id",cll2->room_num,cll1->room_num);
					GW_CL_STATUS_CLS_READY(cll2);

					cll2->old_room_num = cll2->room_num;	// peter, 2019/3/6 save room_num value
					cll2->room_num = cll1->room_num;

				} 
			}
			else	// cl1->room_num == cl2->room_num
			{
				// change coach to merge call / change monitor to merge call

				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_uuid), SWITCH_LOG_DEBUG,
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_DEBUG, "M2 %s HAVE Merge %s\n",cl1->call_name,cl2->call_name);
			}

			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_DEBUG, "param = %s\n",param);

			if(param==NULL)
			{
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_uuid), SWITCH_LOG_NOTICE,
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_INFO, "M2 %s Merge %s\n",cl1->call_name,cl2->call_name);
				
				if(GW_CL_STATUS_IS_MUTE(cl1))		// cl1 -> change from monitor to merge
				{
					gw_call_execute_vcmd(cl1,GW_CL_STATUS_NOT|GW_CL_STATUS_MUTE,"%d unmute $member_id ",cl1->room_num);
					// GW_CL_STATUS_CLS_MUTE(cl1);	// peter 2019/3/5
				}
				
				if(GW_CL_STATUS_IS_MUTE(cl2))		// cl2 -> change from monitor to merge
				{
					gw_call_execute_vcmd(cl2,GW_CL_STATUS_NOT|GW_CL_STATUS_MUTE,"%d unmute $member_id ",cl2->room_num);
					// GW_CL_STATUS_CLS_MUTE(cl2);	// peter 2019/3/5
				}
				
				
				if(GW_CL_STATUS_IS_COACH(cl1))	// cl1 -> change from coach to merge
				{
					if(str_list_in(cl1->coach_id,cl2->str_member_id))
					{
						str_list_del(cl1->coach_id,cl2->str_member_id);
						gw_call_execute_vcmd(cl1,GW_CL_STATUS_NOT|GW_CL_STATUS_COACH,"%d relate %s $member_id clear",cl1->room_num,cl1->nohear_id);
						// GW_CL_STATUS_CLS_COACH(cl1);	// peter 2019/3/5
					}
				}
				
				if(GW_CL_STATUS_IS_COACH(cl2))	// cl2 -> change from coach to merge
				{
					if(str_list_in(cl2->coach_id,cl1->str_member_id))
					{
						str_list_del(cl2->coach_id,cl1->str_member_id);
						gw_call_execute_vcmd(cl2,GW_CL_STATUS_NOT|GW_CL_STATUS_COACH,"%d relate %s $member_id clear",cl2->room_num,cl2->nohear_id);
						// GW_CL_STATUS_CLS_COACH(cl2);	// peter 2019/3/5
					}
				}
				
				if(GW_CL_STATUS_IS(cl1,GW_CL_STATUS_DEAF))
				{
					gw_call_execute_vcmd(cl1,GW_CL_STATUS_NOT|GW_CL_STATUS_DEAF,"%d undeaf $member_id ",cl1->room_num);

					// GW_CL_STATUS_CLS(cl1, GW_CL_STATUS_DEAF);	// peter ¨S³o¦æ, ¸Ó¥[¶Ü? «Ý½T»{
				} 
				
				if(GW_CL_STATUS_IS(cl2,GW_CL_STATUS_DEAF))
				{
					gw_call_execute_vcmd(cl2,GW_CL_STATUS_NOT|GW_CL_STATUS_DEAF,"%d undeaf $member_id ",cl2->room_num);

					// GW_CL_STATUS_CLS(cl2, GW_CL_STATUS_DEAF);	// peter ¨S³o¦æ, ¸Ó¥[¶Ü? «Ý½T»{
				}
			
				// ¨ì¦¹¬°¤î, ¥H¤W³£¥u¦Ò¼{·í¨Æ¤HÂù¤è, ¨S¦Ò¼{¨ì¸s²Õ¤ººÊÅ¥©Î¦Õ»yªº¤H, ¥H¤U¶}©l³B²z

#ifdef MII82
				// process other coach 
				{
					switch_event_header_t *header = NULL;
					gw_conference_room_t * room = gw_find_room(cl1->room_num);
					if(!room || !room->members) 
						return ret;
					
					for (header = room->members->headers; header; header = header->next) 
					{
						if(strcmp(header->name,cl1->call_id) && strcmp(header->name,cl2->call_id))	// «D·í¨ÆÂù¤è	
						{
							gw_call_leg_t * cl = gw_find_callleg(header->name);
							if(cl && GW_CL_STATUS_IS_COACH(cl))
							{
								if(strstr(cl->nohear_id, cl1->call_id) == NULL)		// call_id1 ¬O·sªº party, ¥u­n³B²z call_id1 §Y¥i
								{
									gw_call_execute_vcmd(cl,GW_CL_STATUS_NOT|GW_CL_STATUS_COACH,"%d relate %s $member_id nohear", cl->room_num, cl1->str_member_id);	// just add new party
									// EXEC Call API : [conference 6016 relate  20 19 nohear]  why ????
									// Result:Code:0, relationship 20->19 not found
									if(strstr(cl->error_msg, "not found"))	// failed
									{
										switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_ERROR, "M2 set nohear FAILED: try nospeak\n");
										// gw_call_execute_vcmd(cl,GW_CL_STATUS_NOT|GW_CL_STATUS_COACH,"%d relate $member_id %s nospeak", cl->room_num, cl1->str_member_id);	// just add new party
										// ¤W­±¨º¦æ¥H cl ¬°¥D¤£¦æ, ¤U­±³o¦æ¤w cl1 ¬°¥D¥i¥H, ¦Ó¥B¤U­±³o¦æ¤£·|°¨¤W°µ, ·|µ¥«Ý¤j¬ù 100 ms «á¤~°õ¦æ, ¦]¬° cl1 ÁÙ¦³¨Æ±¡¨S°µ§¹, ÁÙ¦b±Æ¶¤, ¨Ò¦pÁÙ¨S Entry Room, ·íµM·|§ä¤£¨ì
										// ©Ò¥H¥H "½Ö" ¬°¥D«Ü­«­n
	
										gw_call_execute_vcmd(cl1,GW_CL_STATUS_NOT|GW_CL_STATUS_COACH,"%d relate %s $member_id nospeak", cl->room_num, cl->str_member_id);	// just add new party

										if(!strstr(cl1->error_msg, "not found"))
										{
											str_list_add(cl->nohear_id, cl1->str_member_id);
										}
									}
									else
									{
										str_list_add(cl->nohear_id, cl1->str_member_id);
									}

									// don't make GW_CL_STATUS_CLS_COACH
								} // if
							} // if
							
#ifdef MII130
							if(cl && GW_CL_STATUS_IS_MUTE(cl))
							{
								gw_call_execute_vcmd(cl,GW_CL_STATUS_MUTE,"%d mute $member_id ",cl1->room_num);
								// don't make GW_CL_STATUS_SET_MUTE
							}
#endif
						} // if
					} // for
				}
#endif
			}
			else if(strcmp(param,"mute")==0)	// is monitor
			{
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_NOTICE, "M2 %s montior %s\n",cl1->call_name,cl2->call_name);
				if(!GW_CL_STATUS_IS_MUTE(cl1))
				{
					gw_call_execute_vcmd(cl1,GW_CL_STATUS_MUTE,"%d mute $member_id ",cl1->room_num);
					// GW_CL_STATUS_SET_MUTE(cl1);	// peter 2019/3/5
				}
			}
			else if(strcmp(param,"coach")==0)
			{
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_NOTICE, "M2 %s coach %s\n",cl1->call_name,cl2->call_name);
				
				if(member_get_strangers(cl1,cl2))
				{
					if(GW_CL_STATUS_IS_MUTE(cl1))
					{
						gw_call_execute_vcmd(cl1,GW_CL_STATUS_NOT|GW_CL_STATUS_MUTE,"%d unmute $member_id ",cl1->room_num);
						// GW_CL_STATUS_CLS_MUTE(cl1);	// peter 2019/3/5	
					}

					// conference 3100 relate 1 2 nohear
					// Member 1 now cannot hear member 2

					gw_call_execute_vcmd(cl1,GW_CL_STATUS_COACH,"%d relate %s $member_id nohear",cl1->room_num,cl1->nohear_id);
					// GW_CL_STATUS_SET_COACH(cl1);	// peter 2019/3/5
				}
				else
				{
					// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_uuid), SWITCH_LOG_ERROR, "iHMP COACH CALL FAILED: IS NOT READAY %s %s \n",cl1->session_uuid,cl2->session_uuid);
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_ERROR, "M2 COACH CALL FAILED: IS NOT READAY %s %s \n",cl1->session_uuid,cl2->session_uuid);
				}
			}
		}
		else
		{

			if(cl1==NULL)
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not Find Call1 Leg : %s \n",call_id1);


				// send_does_not_exist_evt_notify(call_id1);	// 2023/8/2
				ret = GW_RETURN_RETRY; // 2023/8/2
				return ret;	
			}
			else if(cl1->wait_reinvite)
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Wait For Reinvite Call1 Leg : %s \n",call_id1);
				ret = GW_RETURN_RETRY;
			}

			if(cl2==NULL)
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not Find Call2 Leg : %s \n",call_id2);
				// send_does_not_exist_evt_notify(call_id2);	// 2023/8/4
				ret = GW_RETURN_RETRY;
				return ret;	
			}
			else if(cl2->wait_reinvite){
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Wait For Reinvite Call2 Leg : %s \n",call_id2);
				ret = GW_RETURN_RETRY;
			}

		}
	}
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "M2 Merge Call API : FAILED \n");
	}
	return ret;	
}


static int gw_ev_unmerge_call_handler(switch_event_t *event,int argc ,void * argv[])	// 2023/7/14
{
	int ret= GW_RETURN_OK;
	int member;
	switch_event_t * ev=(switch_event_t*)argv[0];
	cJSON * req = (void*)argv[1];
	
	const char * param = argc > 2 ? (const char *)argv[2] : NULL ;

	const char * call_id1=cJSON_GetObjectCstr(req,"call_id1");


	if(call_id1)
	{
		gw_call_leg_t * cl1=NULL;

		cl1 = gw_find_callleg(call_id1);	// find call leg from hash-table by call_id

		if(cl1 && cl1->wait_reinvite==0)
		{
			switch_core_session_t *rsession = NULL;

			if(!GW_CL_STATUS_IS_READY(cl1))
			{
				if(!GW_CL_STATUS_IS_READY(cl1))	// ¤£·|µo¥Í, unmerge ¤@©w¬O¸Ó cl1 ¤w¸g¦b·|Ä³«Ç¤º
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_ERROR, "M2 UNMERGE_CALL FAILED: IS NOT READAY %s\n", cl1->call_name);

				return GW_RETURN_RETRY;
			}

			// 2023/7/31
			member = gw_get_room_member_count(cl1->room_num);
			if(member == 1)
			{
				// ·|Ä³«Ç¥u¦³¦Û¤v¤@¤H, ¤£¥t«Ø·|Ä³«Ç, ­Y¦³ mute «h unmute §Y¥i

				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_DEBUG, "member = %d, don't create new room\n", member);

				// GW_CL_STATUS_CLS_READY(cl1);  ¤£¯à²M°£¬° not ready 2023/8/1

				if(GW_CL_STATUS_IS_MUTE(cl1))
				{
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_DEBUG, "Auto unmute\n");
					gw_call_execute_vcmd(cl1, GW_CL_STATUS_NOT|GW_CL_STATUS_MUTE, "%d unmute $member_id ", cl1->room_num);
				}

				return GW_RETURN_OK;
			}

			// 2023/7/14  ¯u¥¿ªºÂ÷¶}·|Ä³«Ç

			/*  2022/1/21 ¥H¤U³o¬q´ú¸Õok, ¦ý¬O·í²Ä¤T¤è²¾¥X room «á, ½Ðª`·N­Y³Ñ¤Uªº¨â¤è­Y¦³¤@¤è±¾½u«h·|±¾½u */

			if (!(rsession = switch_core_session_locate(cl1->session_uuid))) 
			{
				//	stream->write_function(stream, "-ERR Cannot locate session!\n");
		
			}
			else	// have 'session'
			{
				// char conference_num[128];
				switch_channel_t *channel = switch_core_session_get_channel(rsession);
				gw_conference_room_t * room=gw_pop_room();

				// gw_call_leg_stop_play(cl1,GW_STOP_PLAY_ALL,__LINE__); ¤£·|¦³³o¼Ëªº±¡ªp

				// transfer ¨ì¤@­Ó·sªº room
				if(room)
				{
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_DEBUG, "room != NULL, member = %d, new room->room_num = %d\n", member, room->room_num);

					// ±N¦¹ cl1 ±q­ì¨Óªº room ¤¤²¾°£

					// sprintf(conference_num, "%d@gw", room->room_num);
					switch_channel_set_variable_printf(channel,"gw_conference_num","%d",room->room_num);

					gw_call_execute_vcmd(cl1,GW_CL_STATUS_MERGE,"%d transfer %d $member_id",cl1->room_num, room->room_num);

					GW_CL_STATUS_CLS_READY(cl1);
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_DEBUG, "set not ready\n");


					if(GW_CL_STATUS_IS_MUTE(cl1))
					{
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl1->session_sn), SWITCH_LOG_DEBUG, "Automatically remove mute flags\n");

						GW_CL_STATUS_CLS_MUTE(cl1);	// 2023/7/31 ¸õ¥X©Ð¶¡¨ì§Oªº©Ð¶¡«á, ·|¦Û°ÊÅÜ¬° unmute
					}

					cl1->old_room_num = cl1->room_num;
					cl1->room_num = room->room_num;
				}
					
				switch_core_session_rwunlock(rsession);		// ¦³«Å§i switch_core_session_t ¨Ã¨ú±o­È®ÉÂ÷¶}¬Ò»Ý©I¥s¦¹¨ç¼ÆÄÀ©ñ
			}

			return ret;
		}
		else
		{
			if(cl1==NULL)
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not Find Call1 Leg : %s \n",call_id1);
				
				// send_does_not_exist_evt_notify(call_id1);	// 2023/8/2
				ret = GW_RETURN_RETRY;
			}
			else if(cl1->wait_reinvite)
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Wait For Reinvite Call1 Leg : %s \n",call_id1);
				ret = GW_RETURN_RETRY;
			}
		}
	}
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "M2 Unmerge Call API : FAILED \n");
	}
	return ret;	
}


static int gw_ev_hangup_ivr_handler(switch_event_t *event,int argc ,void * argv[])
{
//	switch_event_t * ev=(switch_event_t*)argv[0];
	cJSON * req = (void*)argv[1];
	const char * call_id=cJSON_GetObjectCstr(req,"call_id");

	if(call_id){
		gw_call_leg_t * cl=gw_find_callleg(call_id);
		
		if(cl){
			gw_call_leg_stop_play(cl,GW_STOP_PLAY_ALL,__LINE__);
			// FOR UNHLOD
			if(GW_CL_STATUS_IS(cl,GW_CL_STATUS_DEAF))
				gw_call_execute_vcmd(cl,GW_CL_STATUS_NOT|GW_CL_STATUS_DEAF,"%d undeaf $member_id ",cl->room_num);	

		}		
	}else{
		return GW_RETURN_RETRY;
	}

	return GW_RETURN_OK;
}

static int gw_ev_monitor_call_handler(switch_event_t *event,int argc ,void * argv[])
{
	void * argv_[3];
	const char * param="mute";
	
	argv_[0]=argv[0];
	argv_[1]=argv[1];
	argv_[2]=(void*)param;

	return gw_ev_merge_call_handler(event,3,argv_);
}

static int  gw_ev_coach_call_handler(switch_event_t *event,int argc ,void * argv[])
{
	void * argv_[3];
	const char * param="coach";
	
	argv_[0]=argv[0];
	argv_[1]=argv[1];
	argv_[2]=(void*)param;
	
	return gw_ev_merge_call_handler(event,3,argv_);
}


static void gw_call_leg_conf_dtmf(gw_call_leg_t * cl)
{
	if(cl->PlayAll){

		switch_event_header_t *header = NULL;
		gw_call_leg_t* c;

		gw_conference_room_t * room = gw_find_room(cl->room_num);
		if(!room || !room->members) return ;

		for (header = room->members->headers; header; header = header->next) {
			c = gw_find_callleg(header->name);
			if(c){
				if(c!=cl){
					c->want_dtmf_call_id = switch_core_strdup(c->pool,cl->call_id);					
					// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE,"iHMP %s CONF DTMF Event %s \n",c->call_name,cl->call_name);
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,"M2 %s CONF DTMF Event %s \n",c->call_name,cl->call_name);
				}
			}
		}
	}	
}

static void gw_stop_all(gw_call_leg_t * cl)	// 2019/8/5 add by peter
{
}

static void gw_process_play_error(gw_call_leg_t * cl)
{
}

static int gw_ev_play_prompt_handler(switch_event_t *event,int argc ,void * argv[])	// IVR_PLAY_PROMPT_REQ
{
	switch_event_t * ev=(switch_event_t*)argv[0];
	cJSON * req = (void*)argv[1];
	const char * call_id=cJSON_GetObjectCstr(req,"SessionID");
	char arg0[512];
	char arg1[512];
	char arg2[512];
	char arg3[512];
	switch_core_session_t *rsession = NULL;


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "argv[1] = %s\n", (void*)argv[1]);

	// no Unique-ID
	/* ¥H¤U¨âµ§ Event-Sequence ¤£¦P
	{"Event-Name":"CUSTOM","Core-UUID":"684505b1-be73-4f5e-bd2b-79f527cd79cd","FreeSWITCH-Hostname":"iswitch02","FreeSWITCH-Switchname":"iswitch02","FreeSWITCH-IPv4":"172.16.0.92","FreeSWITCH-IPv6":"::1","Event-Date-Local":"2019-03-22 11:04:00","Event-Date-GMT":"Fri, 22 Mar 2019 03:04:00 GMT","Event-Date-Timestamp":"1553223840455100","Event-Calling-File":"mod_gw.c","Event-Calling-Function":"gw_ev_notify_in_handler","Event-Calling-Line-Number":"1685","Event-Sequence":"1096","Event-Subclass":"gw::control","Action":"IVR_PLAY_PROMPT_REQ"}
    {"Event-Name":"CUSTOM","Core-UUID":"684505b1-be73-4f5e-bd2b-79f527cd79cd","FreeSWITCH-Hostname":"iswitch02","FreeSWITCH-Switchname":"iswitch02","FreeSWITCH-IPv4":"172.16.0.92","FreeSWITCH-IPv6":"::1","Event-Date-Local":"2019-03-22 11:03:46","Event-Date-GMT":"Fri, 22 Mar 2019 03:03:46 GMT","Event-Date-Timestamp":"1553223826315108","Event-Calling-File":"mod_gw.c","Event-Calling-Function":"gw_ev_notify_in_handler","Event-Calling-Line-Number":"1685","Event-Sequence":"1013","Event-Subclass":"gw::control","Action":"IVR_PLAY_PROMPT_REQ"}
	*/

	if(ev)
	{
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "coming to gw_ev_play_prompt_handler(), ev\n");	// peter
		// gw_event_dump(ev);	// peter

		// ev
	// {"Event-Name":"NOTIFY_IN","Core-UUID":"61c53908-1cd9-4fc2-b0f7-68280ed259d1","FreeSWITCH-Hostname":"iswitch02","FreeSWITCH-Switchname":"iswitch02","FreeSWITCH-IPv4":"172.16.0.92","FreeSWITCH-IPv6":"::1",
	//	"Event-Date-Local":"2019-03-22 12:20:09","Event-Date-GMT":"Fri, 22 Mar 2019 04:20:09 GMT","Event-Date-Timestamp":"1553228409908400","Event-Calling-File":"sofia.c","Event-Calling-Function":"sofia_handle_sip_i_notify",
	//	"Event-Calling-Line-Number":"557","Event-Sequence":"3174","sip-method":"NOTIFY","pl_data":"{\"system\":\"true\",\"request\":{\"InitialTimeOut_ReturnCode\":\"-1\",\"SessionID\":\"ffebe8afde7355e9424d16d02b2d0dd9@172.16.0.92\",
	//	\"MinKey\":1,\"Domain\":\"system_default\",\"ClearBuffer\":true,\"Barge-in\":false,\"InterDigitTimeOut_ReturnCode\":\"-2\",\"UUID\":\"\",\"TermKey\":\"#\",\"RepeatPlay\":true,\"Name\":\"IVR_PLAY_PROMPT_REQ\",
	//	\"InitialTimeOut\":500,\"DigitMask_ReturnCode\":\"-3\",\"Using-Dtmf\":false,\"DigitMask\":\"1234567890*#\",\"InterDigitTimeOut\":1500,\"ReturnEvt\":\"true\",\"FileList\":[\"HoldMusic.wav\"],\"Language Type\":\"TW\",
	//	\"MaxKey\":12}}","content-type":"application/json","from-uri":"sip:iswitch@172.16.0.92:5060","to-uri":"sip:mcs@172.16.0.92:5070","call-id":"124975998031046050767420962048","Content-Length":"502",
	//	"_body":"{\"system\":\"true\",\"request\":{\"InitialTimeOut_ReturnCode\":\"-1\",\"SessionID\":\"ffebe8afde7355e9424d16d02b2d0dd9@172.16.0.92\",\"MinKey\":1,\"Domain\":\"system_default\",\"ClearBuffer\":true,
	//	\"Barge-in\":false,\"InterDigitTimeOut_ReturnCode\":\"-2\",\"UUID\":\"\",\"TermKey\":\"#\",\"RepeatPlay\":true,\"Name\":\"IVR_PLAY_PROMPT_REQ\",\"InitialTimeOut\":500,\"DigitMask_ReturnCode\":\"-3\",\"Using-Dtmf\":false,
	//	\"DigitMask\":\"1234567890*#\",\"InterDigitTimeOut\":1500,\"ReturnEvt\":\"true\",\"FileList\":[\"HoldMusic.wav\"],\"Language Type\":\"TW\",\"MaxKey\":12}}"}
	}

	if(call_id)
	{
		int i,j;
		gw_call_leg_t * cl=NULL;
		cJSON * fl=NULL;
		cJSON * n=NULL;
		int members;

		cl = gw_find_callleg(call_id);
		
		if(cl)
		{
			int  PlayAll;
			char *dtmf_ignore_ms;

			PlayAll=cJSON_GetObjectInt(req,"Conference",0);
			if(PlayAll)
			{
				// is Conference=1, °±¤î©Ò¦³ member ¤§play/mute/deaf
				// MII-137, 2019/8/27 ­×¥¿, «O¯d¤¤ªº¤@¤è¤£¯à³Q merge ¶i¨Ó
				gw_stop_all(cl);
			}

			cl->InitialTimeOut_ReturnCode	= cJSON_GetObjectInt(req,"InitialTimeOut_ReturnCode",-1);
			cl->InterDigitTimeOut_ReturnCode= cJSON_GetObjectInt(req,"InterDigitTimeOut_ReturnCode",-2);
			cl->DigitMask_ReturnCode		= cJSON_GetObjectInt(req,"DigitMask_ReturnCode",-3);
			cl->InterDigitTimeOut			= 1000*cJSON_GetObjectInt(req,"InterDigitTimeOut",0);
			cl->InitialTimeOut				= 1000*cJSON_GetObjectInt(req,"InitialTimeOut",0);

			cl->PlayAll						= cJSON_GetObjectInt(req,"Conference",0);
	
			cl->MinKey						= cJSON_GetObjectInt(req,"MinKey",1);
			
			cJSON_GetObjectStrcpy(req,"Domain",cl->Domain,sizeof(cl->Domain),"system_default");

			cl->ClearBuffer					= cJSON_GetObjectInt(req,"ClearBuffer",1);
			cl->BargeIn						= cJSON_GetObjectInt(req,"Barge-in",0);

			cJSON_GetObjectStrcpy(req,"TermKey",cl->TermKey,sizeof(cl->TermKey),"");
			
			cl->RepeatPlay					= cJSON_GetObjectInt(req,"RepeatPlay",0);
			cl->UsingDtmf					= cJSON_GetObjectInt(req,"Using-Dtmf",0);
			cl->Security					= cJSON_GetObjectInt(req,"Security",0);		// 2019/8/22
			cl->ReturnEvt					= cJSON_GetObjectInt(req,"ReturnEvt",0);
			cl->MaxKey						= cJSON_GetObjectInt(req,"MaxKey",12);


			cl->ASR							= cJSON_GetObjectInt(req,"ASR",0);			// 2022/7/12

			// for test
			// cl->ASR = 1;

			cJSON_GetObjectStrcpy(req,"DigitMask",cl->ASRType, sizeof(cl->ASRType)-1,"Generic");	// 2022/7/12


#ifdef MII132
			if(cl->MaxKey < cl->MinKey)		// peter 2019/4/29, ·í cl->MaxKey = -1 ®É·|·í¾÷
				cl->MaxKey = 99;
#endif
			
			if(cl->MaxKey > 1 && cl->InterDigitTimeOut <=0)			// 2020/5/25
				cl->InterDigitTimeOut = 5000000;	// InterDigitTimeOut <=0 ¬O¤£¦X²zªº, ±j¨îÂà´«¬° 5 ¬í

			if(cl->BargeIn == 0 && cl->InitialTimeOut == 99000000)	// 2020/5/25
				cl->InitialTimeOut = 0;

			cJSON_GetObjectStrcpy(req,"DigitMask",cl->DigitMask,sizeof(cl->DigitMask),"1234567890*#");
			cJSON_GetObjectStrcpy(req,"Language Type",cl->LanguageType,sizeof(cl->LanguageType),"TW");
			cJSON_GetObjectStrcpy(req,"Name",cl->LastReq,sizeof(cl->LastReq),"");
			
			sprintf(cl->MySound_prefix,"%s/audio/%s/prompt1/",cl->Domain,cl->LanguageType);

			fl = cJSON_GetObjectItem(req,"FileList");
			
			cl->play_error=0;

			/* 2020/9/22 ¸ó block ¤]­n²M°£´Ý¯d«öÁä, §_«h³Ì«á¤@­Ó«öÁä¦³¥i¯à±a¨ì¤U¤@­Ó block ¦Ó³Q·í§@²Ä¤@­Ó«öÁä
			// 2020/9/2 peter
			cl->dtmf_last_ts = 0;	// clean
			cl->dtmf_last[0] = 0;	// clean
			cl->dtmf_ignore_ms =0;	// clean
			*/
			
			dtmf_ignore_ms = switch_core_get_variable("dtmf_ignore_ms");
			if (!zstr(dtmf_ignore_ms))
				cl->dtmf_ignore_ms = atoi(dtmf_ignore_ms) * 1000;
			if(cl->dtmf_ignore_ms < 0)
				cl->dtmf_ignore_ms = 0;

			// for QQ test append #*
			strcat(cl->DigitMask,cl->TermKey);

			cl->File_idx= cl->nFileList = 0;

			if(fl)
				j = cJSON_GetArraySize(fl);
			else 
				j=0;

			// get filename
			for(i=0;i<j && cl->nFileList <MAX_PLAYLIST;i++)
			{
				n = cJSON_GetArrayItem(fl,i);
				if(n &&  n->type == cJSON_String && n->valuestring )
				{
					if(n->valuestring && strlen(n->valuestring)<128 )
					{
						switch_snprintf(cl->FileList[cl->nFileList],128,"%s",n->valuestring);
						cl->nFileList++;
					}
					else 
					{
						// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_ERROR, 
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_ERROR, "M2 play file path is too length \n");
					}
				}
			}

			switch_snprintf(cl->play_cookie,sizeof(cl->play_cookie),"%d@%d",cl->play_req_sn++,cl->member_id);

			cl->want_dtmf_call_id=NULL;// disable 
			
			if(cl->BargeIn && cl->ReturnEvt && cl->UsingDtmf==0)
			{
				//for peter request 
				// press any key to break and return event
				// 
				strcpy(cl->DigitMask,"1234567890*#");
				cl->MaxKey=1;
				cl->UsingDtmf=2;
			}
			
			if(cl->UsingDtmf)
			{
				// FIX MII-64
				if(cl->ClearBuffer )
				{
					cl->dtmf_idx=0;
					cl->dtmf_buf[0]=0;
				}
				//disable InitialTimeOut;
				// cl->InitialTimeOut=-1;	// why why why
			}
			else
			{
				cl->dtmf_idx=0;
				cl->dtmf_buf[0]=0;
			}

			// 
			members = gw_get_room_member_count(cl->room_num);
		
			if(cl->PlayAll)				// PlayAll=cJSON_GetObjectInt(req,"Conference",0);
			{
				gw_call_leg_conf_dtmf(cl);
			}
			else
			{
				//FOR CONSULT ONLY HEAR AUDIO FILE 
				// AND MUTE 
				if(gw_get_room_member_count(cl->room_num)>1) 
				{
					if(!GW_CL_STATUS_IS(cl,GW_CL_STATUS_DEAF))
						gw_call_execute_vcmd(cl,GW_CL_STATUS_DEAF,"%d deaf $member_id ",cl->room_num);
					
					if(!GW_CL_STATUS_IS_MUTE(cl))
					{
						gw_call_execute_vcmd(cl,GW_CL_STATUS_MUTE,"%d mute $member_id ",cl->room_num);						
						// GW_CL_STATUS_SET_MUTE(cl);	// peter 2019/3/5	
					}
				}
			}

			if(ev){
				if(switch_event_get_header(ev,"X-GW-REQ")){
					cl->x_call_id = switch_core_strdup(cl->pool,switch_event_get_header(ev, "call-id"));
				}
				cl->contact_uri = switch_core_strdup(cl->pool,switch_event_get_header(ev, "contact-uri"));
				cl->from_uri = switch_core_strdup(cl->pool,switch_event_get_header(ev, "from-uri"));
				cl->to_uri = switch_core_strdup(cl->pool,switch_event_get_header(ev, "to-uri"));
				if(zstr( cl->contact_uri) ){
					cl->contact_uri=switch_core_strdup(cl->pool,cl->from_uri);
				}
			}

			gw_call_leg_stop_play(cl,GW_STOP_PLAY_CURRENT,__LINE__);	// stop 


			if(cl->is_ai_service)
			{
			}

			gw_call_leg_play(cl,__LINE__);			// play

			//if(cl->BargeIn==0 && cl->File_idx == cl->nFileList-1 && strcasecmp(cl->FileList[cl->File_idx], "Noise.wav")==0)	// 2020/5/20 ¥u¼½©ñ¤@­Ó¤zÂZ­µ®É	// 2023/12/15
			if(cl->BargeIn==0 && cl->File_idx == cl->nFileList-1 && strncasecmp(cl->FileList[cl->File_idx], "Noise", 5)==0)		// 2023/12/15
			{
				char *new_inputpw;

				new_inputpw = switch_core_get_variable("new_inputpw");

				if( new_inputpw && switch_true(new_inputpw))
				{
					gw_call_leg_start_timer(cl);
				}
			}

#ifdef MII131
			if(strstr(cl->error_msg,"not found"))
			{
				gw_process_play_error(cl);
				return GW_RETURN_OK;
			}
#endif

#ifdef MII81
			if(cl->dtmf_idx){
				char retry_key[2]={cl->dtmf_buf[cl->dtmf_idx-1],0};
				
				cl->dtmf_buf[cl->dtmf_idx-1]=0x0;
				cl->dtmf_idx--;

				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, 
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "M2 %s Retry DTMF Event %s  \n",cl->call_name,retry_key);	
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "M2 Retry DTMF Event %s  \n", retry_key);	
				gw_call_leg_dtmf_handler(cl,retry_key,0);
			}
#endif 
		}
		else
		{
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "M2 Can Not Found Call Leg %s \n",call_id);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "M2 Can Not Found Call Leg %s \n",call_id);
			return GW_RETURN_RETRY;
		}

	}
	else
	{
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "M2 Can Not Found SessionID\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "M2 Can Not Found SessionID\n");
	}

	return GW_RETURN_OK;
}

static int gw_ev_add_member_handler(switch_event_t *event,int argc ,void * argv[])
{
	//get member id
	switch_core_session_t *session =NULL;
	const char * uuid;
	gw_conference_room_t * room;

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "coming to gw_ev_add_member_handler()\n");	// peter
	// gw_event_dump(event);	// peter
	// have Unique-ID

	// have uuid
	uuid=switch_event_get_header(event, "Unique-ID");

	room = gw_find_room_by_event(event);

	if(!room) 
		return GW_RETURN_OK;

	session=uuid?switch_core_session_locate(uuid):NULL;
	if(session){
		const char * call_id;
		gw_call_leg_t * cl=NULL;
		switch_channel_t *channel = switch_core_session_get_channel(session);
		
		call_id = switch_channel_get_variable(channel,"gw_call_id");
		if(call_id){
			
			if(room->members==NULL){
				switch_event_create_subclass(&room->members, SWITCH_EVENT_CLONE, NULL);
			}			

			cl = gw_find_callleg(call_id);
			
			if(cl)
			{
				// 2024/2/1
				switch_stream_handle_t mystream = { 0 };
				char command[100];
				char mess[1000];
				char *ai_service_number1 = NULL;
				char *ai_service_number2 = NULL;
				char *ai_service_number3 = NULL;
				char *ai_server_ip = NULL;	// 2024/4/16

				char *sip_to_user = NULL;
				char *sip_caller = NULL;
				//

				cl->room_num= atoi(switch_event_get_header(event, "Conference-Name"));		// 6006

				strcpy(cl->str_member_id,switch_event_get_header(event, "Member-ID"));		// "7"
				cl->member_id = atoi(cl->str_member_id);									// 7

				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, 
				//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, 
				//	"iHMP %s Entry Room %s@%d \n",cl->call_name,cl->str_member_id,cl->room_num);		// 7@6006
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "M2 Entry Room %s@%d \n", cl->str_member_id,cl->room_num);		// 7@6006

				switch_event_add_header(room->members, SWITCH_STACK_BOTTOM, call_id, "%d",cl->member_id);
				
				GW_CL_STATUS_SET_READY(cl);
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "set ready\n");

				// 2023/8/1
				cl->channel_active = 1;

				gw_call_execute_wait_cmds(cl);	// Entry Room 7@6006

				ai_service_number1 = switch_core_get_variable("ai_service_number1");
				ai_service_number2 = switch_core_get_variable("ai_service_number2");
				ai_service_number3 = switch_core_get_variable("ai_service_number3");

				sip_to_user = switch_event_get_header(event, "Caller-Destination-Number");	// ª`·N, ¸ò¤W¤@¦æ¤£¤@¼Ë, ³o¬O event ¤ºªºÅÜ¼Æ
				sip_caller = switch_event_get_header(event, "Caller-ANI");

				strncpy(cl->sip_ANI, sip_caller, 64);
				strncpy(cl->sip_DNIS, sip_to_user, 64);

				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "ai_service_number = %s\n", ai_service_number);
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "sip_to_user = %s\n", sip_to_user);
				
				if(ai_service_number1 != NULL && sip_to_user != NULL && strlen(ai_service_number1) > 0 && strcmp(sip_to_user, ai_service_number1) == 0)
				{
					ai_server_ip = switch_core_get_variable("ai_server_ip1");

					cl->is_ai_service = 1;
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "Is AI service number1 : %s\n", ai_service_number1);
				}
				if(ai_service_number2 != NULL && sip_to_user != NULL && strlen(ai_service_number2) > 0 && strcmp(sip_to_user, ai_service_number2) == 0)
				{
					ai_server_ip = switch_core_get_variable("ai_server_ip2");
					cl->is_ai_service = 1;
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "Is AI service number2 : %s\n", ai_service_number2);
				}
				if(ai_service_number3 != NULL && sip_to_user != NULL && strlen(ai_service_number3) > 0 && strcmp(sip_to_user, ai_service_number3) == 0)
				{
					ai_server_ip = switch_core_get_variable("ai_server_ip3");
					cl->is_ai_service = 1;
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "Is AI service number3 : %s\n", ai_service_number3);
				}
				if(cl->is_ai_service)
				{
					// 2024/2/1
					SWITCH_STANDARD_STREAM(mystream);

					sprintf(command, "uuid_audio_stream");
					sprintf(mess, "%s start ws://%s/asr mono 16k \"uuid\":\"%s\",\"Type\":\"flowInit\",\"Flow\":\"10000\"", cl->session_uuid, ai_server_ip, cl->session_uuid);
	
					switch_api_execute(command, mess, NULL, &mystream);
					if (mystream.data) 
					{
					}

					switch_safe_free(mystream.data);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "call uuid_audio_stream %s\n", mess);
					//
						
					if(GW_CL_STATUS_IS(cl,GW_CL_STATUS_TIMER)){
						cl->InterDigit_ts = cl->dtmf_Initial_ts = switch_time_ref();
					}
				}
			}
		}
		
		switch_core_session_rwunlock(session);

	}else
	{
		//gw_event_dump(event);
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "M2 Add Member Can Not Found Unique-ID  \n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "M2 Add Member Can Not Found Unique-ID  \n");
	}

	return GW_RETURN_OK;
}

static int gw_ev_del_member_handler(switch_event_t *event,int argc ,void * argv[])
{
	//get member id
	switch_core_session_t *session =NULL;
	const char * uuid=switch_event_get_header(event, "Unique-ID");
	gw_conference_room_t * room = gw_find_room_by_event(event);


	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "coming to gw_ev_del_member_handler()\n");	// peter
	// gw_event_dump(event);	// peter

	// have Unique-ID


	if(!room) 
		return GW_RETURN_OK;

	session=uuid?switch_core_session_locate(uuid):NULL;

	if(session){
		const char * call_id;
		gw_call_leg_t * cl=NULL;
		switch_channel_t *channel = switch_core_session_get_channel(session);
		
		call_id = switch_channel_get_variable(channel,"gw_call_id");
		if(call_id){
			cl = gw_find_callleg(call_id);
			if(cl)
				cl->member_id = 0;
			if(room->members){
				switch_event_del_header(room->members,call_id);
			}
			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, 
			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, 
			//	"iHMP %s Leave Room %s@%d \n",cl->call_name,cl->str_member_id,cl->room_num);

			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "M2 %s Leave Room %s@%d \n",cl->call_name,cl->str_member_id,cl->old_room_num);
			switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "M2 Leave Room %s@%d \n", cl->str_member_id,cl->old_room_num);
		}
		switch_core_session_rwunlock(session);
	}else{
		//gw_event_dump(event);
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "iHMP Delete Member Can Not Found Unique-ID  \n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "M2 Delete Member Can Not Found Unique-ID  \n");	// peter
	}

	return GW_RETURN_OK;
}

static int gw_ev_reinvite_handler(switch_event_t *event,int argc ,void * argv[])
{
	switch_core_session_t *session =NULL;
	const char * uuid=switch_event_get_header(event, "Unique-ID");

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "coming to gw_ev_reinvite_handler()\n");	// peter
	// gw_event_dump(event);	// peter

	// no log for refer

	session = uuid?switch_core_session_locate(uuid):NULL;
	if(session){
		const char * call_id;
		gw_call_leg_t * cl=NULL;
		switch_channel_t *channel = switch_core_session_get_channel(session);
			
		call_id = channel? switch_channel_get_variable(channel,"gw_call_id"):NULL;
		if(call_id){
	
			cl = gw_find_callleg(call_id);
			if(cl){
				
				const char * r_ip = switch_channel_get_variable(channel, SWITCH_REMOTE_MEDIA_IP_VARIABLE);
				if(cl->wait_reinvite){
					cl->wait_reinvite = 0;
					if(strcmp("0.0.0.0",r_ip)==0){
						cl->wait_reinvite=1;
					}
				}
				//
				// FIXBUG:MII-60 
				// reinvite to stop inband dtmf detect
				//
#ifdef MII60 	
				{
					//int idtmf_type = SWITCH_DTMF_INBAND_AUDIO;
					const char * dtmf_type= switch_channel_get_variable(channel,"dtmf_type");
					const char * r_sdp =  switch_channel_get_variable(channel,"switch_r_sdp");
					
					
					if( dtmf_type && strcmp(dtmf_type,"rfc2833")==0)
						cl->dtmf_type=SWITCH_DTMF_RTP;
					if(cl->dtmf_type!= SWITCH_DTMF_RTP && strstr(r_sdp,"telephone-event/"))
						cl->dtmf_type=SWITCH_DTMF_RTP;

				}
#endif 
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, 
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "M2 %s Reinvite %d %s\n",cl->call_name,cl->wait_reinvite,r_ip);
				switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE, "M2 Reinvite %d %s\n", cl->wait_reinvite,r_ip);

				gw_call_execute_wait_cmds(cl);
			}
		}
		switch_core_session_rwunlock(session);
	}else{
		//gw_event_dump(event);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "M2 Reinvite Can Not Found Unique-ID  \n");		
	}

	return GW_RETURN_OK;
}

static int gw_ev_conference_destroy_handler(switch_event_t *event,int argc ,void * argv[])
{
	gw_conference_room_t * room = gw_find_room_by_event(event);

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "coming to gw_ev_conference_destroy_handler()\n");	// peter
	// gw_event_dump(event);	// peter

	// no Unique-ID
	//
	//  {"Event-Subclass":"conference::maintenance","Event-Name":"CUSTOM","Core-UUID":"684505b1-be73-4f5e-bd2b-79f527cd79cd","FreeSWITCH-Hostname":"iswitch02","FreeSWITCH-Switchname":"iswitch02","FreeSWITCH-IPv4":"172.16.0.92","FreeSWITCH-IPv6":"::1","Event-Date-Local":"2019-03-22 11:04:03","Event-Date-GMT":"Fri, 22 Mar 2019 03:04:03 GMT","Event-Date-Timestamp":"1553223843655588","Event-Calling-File":"mod_conference.c","Event-Calling-Function":"conference_thread_run","Event-Calling-Line-Number":"730","Event-Sequence":"1144","Conference-Name":"6004","Conference-Size":"0","Conference-Ghosts":"0","Conference-Profile-Name":"gw","Conference-Unique-ID":"a521697e-cd71-4a00-a783-796404e1b84f","Action":"conference-destroy"}


	if(room){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "M2 Push back Room:%s \n",room->name);
		if(room->members){
			switch_event_destroy(&room->members);
			room->members=NULL;
		}
		gw_push_room(room);
	}

	return GW_RETURN_OK;
}

static int gw_ev_play_file_done_handler(switch_event_t *event,int argc ,void * argv[])
{
	switch_core_session_t *session =NULL;
	const char * uuid=switch_event_get_header(event, "Unique-ID");
	
	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "coming to gw_ev_play_file_done_handler()\n");	// peter
	// gw_event_dump(event);	// peter
	
	// maybe no Unique-ID ???????
	// {"Event-Subclass":"conference::maintenance","Event-Name":"CUSTOM","Core-UUID":"684505b1-be73-4f5e-bd2b-79f527cd79cd","FreeSWITCH-Hostname":"iswitch02","FreeSWITCH-Switchname":"iswitch02","FreeSWITCH-IPv4":"172.16.0.92","FreeSWITCH-IPv6":"::1","Event-Date-Local":"2019-03-22 11:04:03","Event-Date-GMT":"Fri, 22 Mar 2019 03:04:03 GMT","Event-Date-Timestamp":"1553223843635092","Event-Calling-File":"conference_file.c","Event-Calling-Function":"conference_file_close","Event-Calling-Line-Number":"51","Event-Sequence":"1114","Conference-Name":"6004","Conference-Size":"2","Conference-Ghosts":"0","Conference-Profile-Name":"gw","Conference-Unique-ID":"a521697e-cd71-4a00-a783-796404e1b84f","seconds":"8","milliseconds":"8192","samples":"65536","play-cookie":"0@6","Action":"play-file-member-done","File":"{play-cookie=0@6}/mmcc/domain/system_default/audio/TW/prompt1/HoldMusic.wav"}
	
	
	if(uuid)
		session=switch_core_session_locate(uuid);
	else{
		// gw_event_dump(event);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "M2 Play done Can Not Found Unique-ID IS NULL \n");	
		return GW_RETURN_OK;
	}
	if(session){
		const char * call_id;
		gw_call_leg_t * cl=NULL;
		switch_channel_t *channel = switch_core_session_get_channel(session);
		
		call_id = switch_channel_get_variable(channel,"gw_call_id");
		if(call_id){
			cl = gw_find_callleg(call_id);
			
			if(cl)
			{
				const char * play_cookie=switch_event_get_header(event, "play-cookie");
				
				if(GW_CL_STATUS_IS(cl, GW_CL_STATUS_BREAK_PLAY))	// 2019/8/1 add by peter
				{
					GW_CL_STATUS_CLS(cl, GW_CL_STATUS_BREAK_PLAY);
				}
				else
				{
					if(play_cookie && !strcmp(play_cookie,cl->play_cookie))
					{
						const char * error_msg = switch_event_get_header(event, "Play-Error");	// peter 2019/5/30, ¨S³]¦¹°Ñ¼Æ
						char *new_inputpw;
						
						if(error_msg)
							cl->play_error=1;
						
						GW_CL_STATUS_CLS_PLAYING(cl);
						
						cl->File_idx++;
						
						//switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE, 
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_INFO, "M2 Play done cookie=%s idx=%d nFileList=%d RepeatPlay=%d ReturnEvt=%d\n", play_cookie,cl->File_idx,cl->nFileList,cl->RepeatPlay,cl->ReturnEvt);

						new_inputpw = NULL;

						// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "cl->BargeIn = %d\n", cl->BargeIn);

						// if(cl->BargeIn==0)
						if(cl->BargeIn != 1)	// BargeIn =0 ®É­Y¦³«öÁä·|¸õ =2
						{
							new_inputpw = switch_core_get_variable("new_inputpw");
							//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "new_inputpw %s \n", new_inputpw);

							//if(new_inputpw && switch_true(new_inputpw))
							//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "switch_true(new_inputpw) is true\n");
							//else
							//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "switch_true(new_inputpw) is false\n");
						}

						// if( new_inputpw && switch_true(new_inputpw) && cl->BargeIn !=1 && cl->File_idx == cl->nFileList  && strcasecmp(cl->FileList[cl->File_idx-1], "Noise.wav")==0 )	// 2020/5/18 ¤zÂZ­µ¤w¼½§¹, 2020/5/20 ¥[ cl->BargeIn==0 (¤]¥i¯à =2) §PÂ_, ¥H§K¥x·s»Ý­×§ï¬yµ{		// 2023/12/15
						if( new_inputpw && switch_true(new_inputpw) && cl->BargeIn !=1 && cl->File_idx == cl->nFileList  && strncasecmp(cl->FileList[cl->File_idx-1], "Noise", 5)==0 )		// 2023/12/15
						{
							// auto repeat
							cl->File_idx--;
							gw_call_leg_play(cl,__LINE__);
						}
						else
						{
							if( cl->File_idx < cl->nFileList )
							{
								gw_call_leg_play(cl,__LINE__);

#ifdef MII131
								if(strstr(cl->error_msg,"not found"))
								{
									gw_process_play_error(cl);

									switch_core_session_rwunlock(session);		// 2023/1/16, ¨S¦³¦¹¦æ¯Ê­µÀÉ®É session ·|¥d¦í
									
									return GW_RETURN_OK;
								}
#endif
								
								// ·í play ²Ä¤@­Ó­µÀÉ´N«ö¤U«öÁä®É, cl->BargeIn ·|³Q§ï¬° 2, ¤U­±ªº±ø¥ó´N¤£·|¦¨¥ß, ¸g trace «áµo²{¤£¬O cl->FileList[cl->File_idx] ¤£¬° Noise.wav 
								// cl->BargeIn==0 ©Î ==2 ¤£­«­n, ³Ì­«­nªº¬O¬° Noise.wav ­nÄ²µo
								// add at 2020/3/27
								// if(cl->BargeIn==0 && cl->File_idx == cl->nFileList-1 && strcasecmp(cl->FileList[cl->File_idx], "Noise.wav")==0)		// ¼½©ñ¤zÂZ­µ®É¶}©l­p®É, 2020/5/20 µo²{·í Noise «D¦b²Ä¤@­Ó­µÀÉ¤~·|¶]¨ì³o, ­Y¥x·sªº²Ä¤@­Ó­µÀÉ¬° Noise.wav ´N¤£·|¶]¨ì³o
								// if(cl->BargeIn != 1 && cl->File_idx == cl->nFileList-1 && strcasecmp(cl->FileList[cl->File_idx], "Noise.wav")==0)	// 2020/5/25	// 2023/12/15
								if(cl->BargeIn != 1 && cl->File_idx == cl->nFileList-1 && strncasecmp(cl->FileList[cl->File_idx], "Noise", 5)==0)		// 2023/12/15
								{
									// gw_call_leg_start_timer(cl);  §ï¬°¥H¤U, ·sªº°µªk¬O³o¸Ì¶}©l­p®É, ÂÂªº°µªk¤£²z·|

									//
									if( new_inputpw && switch_true(new_inputpw))
									{
									//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "gw_call_leg_start_timer\n");
										gw_call_leg_start_timer(cl);
									}

								}
#ifdef MII94 
							}else if(cl->nFileList == 1 && cl->play_error){
								// only one file and play eror 
								
								// À³¸Ó¤£·|¶]¨ì³o
								gw_report_dtmf_event(cl,GW_PLAY_CODE_DONE,1,__LINE__);
#endif 						
							}else if(cl->RepeatPlay){
								cl->File_idx=0;
								gw_call_leg_play(cl,__LINE__);
							}else if(cl->ReturnEvt && cl->UsingDtmf==0) {
								gw_report_dtmf_event(cl,GW_PLAY_CODE_DONE,1,__LINE__);
							}else if(cl->ReturnEvt && cl->UsingDtmf==2) {
								gw_report_dtmf_event(cl,GW_PLAY_CODE_DONE,1,__LINE__);
							}else if(cl->UsingDtmf){
								//start timer
#ifdef MII90						
								if(cl->InterDigitTimeOut ==0 )
									gw_report_dtmf_event(cl,GW_PLAY_CODE_INPUT_DIGIT_TIMEOUT,1,__LINE__);
							//	else if(cl->InitialTimeOut ==0 )	// 2020/5/25 ¦³¥i¯à¬O -1*1000 = -1000
								else if(cl->InitialTimeOut <=0 )
									gw_report_dtmf_event(cl,GW_PLAY_CODE_INPUT_TIMEOUT,1,__LINE__);
								else
#endif
									gw_call_leg_start_timer(cl);	
							}
						}
					}
					else 
					{
						// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_DEBUG, 
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_DEBUG, 
							"M2 Play done %s cookie is invalid %s \n",call_id,play_cookie?play_cookie:"no play_cookie");			
					}
				}
			}else{
				// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "M2 Play done Can Not found call leg %s  \n",call_id);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "M2 Play done Can Not found call leg %s  \n",call_id);
			}
		}else{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
				"M2 Play done Can Not found gw_call_id  \n");			
		}
		switch_core_session_rwunlock(session);
	}else{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
			"M2 Play done Can Not Session  \n");			
	}
	return GW_RETURN_OK;
}

static int gw_ev_conf_play_file_done_handler(switch_event_t *event,int argc ,void * argv[])
{
	const char * uuid=switch_event_get_header(event, "Unique-ID");

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "coming to gw_ev_conf_play_file_done_handler()\n");	// peter
	// gw_event_dump(event);
	// no log for refer

	if(uuid){
		return gw_ev_play_file_done_handler(event,argc,argv);
	}else{
		gw_conference_room_t * room = gw_find_room_by_event(event);

	//	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Entry %s \n",__FUNCTION__);

		if(room && room->members){
			switch_event_header_t *header = NULL;
			gw_call_leg_t* cl=NULL;
			const char * play_cookie=switch_event_get_header(event, "play-cookie");
			
			for (header = room->members->headers; header; header = header->next) {
				cl = gw_find_callleg(header->name);
				if(cl && play_cookie && !strcmp(play_cookie,cl->play_cookie)){
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM,"Unique-ID",cl->session_uuid);
					gw_ev_play_file_done_handler(event,argc,argv);
					break;
				}
				cl=NULL;
			}
				
		}else{
			//gw_event_dump(event);
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "M2 Play done Can Not Found ROOM IS NULL \n");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "M2 Play done Can Not Found ROOM IS NULL \n");
		}		
	}

	return GW_RETURN_OK;
}

static void gw_call_leg_dtmf_handler(gw_call_leg_t * cl,const char * key,int dtmf_duration)
{

	// ¨C«ö¤@­Ó DTMF ³£·|¶]¨ì³o

	if(!cl || !key)
		return ;

	if(cl->send_dtmf_event)	// ·|Ä³«Ç¤¤«ö¤F DTMF
	{
		send_dtmf_notify(cl, key[0]);
		return;
	}

	// ¦b³o¸Ì³B²z¿ý­µ®Éªº«öÁä 2019/12/4
	if(GW_CL_STATUS_IS(cl,GW_CL_STATUS_BLOCK_RECORD))
	{
		if(strchr(cl->RecTermKey, key[0]))	// «öÁäµ²§ô¿ý­µ
		{
			// stop record
			// gw_party_record_stop(cl, cl->pRecordPath, cl->pRecordFile);
			gw_party_record_stop(cl);		// 2022/10/7

		//	GW_CL_STATUS_CLS(cl, GW_CL_STATUS_BLOCK_RECORD); ¤w¦b¤W­± func ¤º°µ
		}

		return;
	}

	if(GW_CL_STATUS_IS_PLAYING(cl))
	{
		// do 
		if(cl->BargeIn==1){
			gw_call_leg_stop_play(cl,GW_STOP_PLAY_ALL,__LINE__);
			cl->RepeatPlay=0;
			cl->File_idx = cl->nFileList=0;
			cl->BargeIn=3;
		}else if(cl->BargeIn==0) {
#ifdef MII104			
			if(cl->UsingDtmf )
#endif 			
			{
				if(cl->nFileList>1 && cl->File_idx != cl->nFileList-1){
					cl->File_idx = cl->nFileList-2;
					gw_call_leg_stop_play(cl,GW_STOP_PLAY_CURRENT,__LINE__);
				}
				cl->RepeatPlay=0;
				cl->BargeIn=2;

			}
		}
	}

	if(cl->UsingDtmf )
	{
		cl->last_input[0]=key[0];
		cl->last_input[1]=0x0;

		if(strlen(cl->DigitMask) ==0  ||  strchr(cl->DigitMask, key[0])){
			// update inter digit timestamp
			cl->InterDigit_ts = switch_time_ref();
			//start timer
			gw_call_leg_start_timer(cl);
			// 
			
			if(strlen(cl->TermKey) && strchr(cl->TermKey, key[0]) ) {
				//find term key 
				// report dtmf event;
				gw_report_dtmf_event(cl,GW_PLAY_CODE_TERM_KEY,1,__LINE__);
				return ;
			}

			if(cl->dtmf_idx<127){
				cl->dtmf_buf[cl->dtmf_idx]=key[0];
				cl->dtmf_idx++;
				cl->dtmf_buf[cl->dtmf_idx]=0x0;
			}

			if(cl->dtmf_idx >=cl->MaxKey){
				//report dtmf Event;
				gw_report_dtmf_event(cl,GW_PLAY_CODE_MAX_KEY,1,__LINE__);
				return ;
			}

		}else if(strlen(cl->DigitMask) && strchr(cl->DigitMask, key[0])==NULL ){
			//fixbug MII102
#ifdef  MII102			
			if(strlen(cl->TermKey) && strchr(cl->TermKey, key[0]) ) {
				//find term key 
				// report dtmf event;
				gw_report_dtmf_event(cl,GW_PLAY_CODE_TERM_KEY,1,__LINE__);
				return ;
			}			
#endif
			gw_report_dtmf_event(cl,GW_PLAY_CODE_KEY_MISS_MASK,1,__LINE__);
		}
	}else if(dtmf_duration !=0 && cl->want_dtmf_call_id){
		gw_call_leg_t * c= gw_find_callleg(cl->want_dtmf_call_id);
		if(c){
			gw_call_leg_dtmf_handler(c,key,0);
		}
	}else if(dtmf_duration !=0 ){
		//MII-49 :

		switch_event_header_t *header = NULL;

		gw_conference_room_t * room = gw_find_room(cl->room_num);

		//record dtmf
		if(cl->dtmf_idx<127){
			cl->dtmf_buf[cl->dtmf_idx]=key[0];
			cl->dtmf_idx++;		
			cl->dtmf_buf[cl->dtmf_idx]=0x0;
		}

		if(cl->dtmf_type != SWITCH_DTMF_RTP) // dtmf is rfc2833 ,so bridge dtmf
			return ;

		if(!room || !room->members) return ;

		if(dtmf_duration>400)
			dtmf_duration = 400;
		for (header = room->members->headers; header; header = header->next) {
			gw_call_leg_t* c = gw_find_callleg(header->name);
			if(c){
				if(c!=cl){
					//dtmf	 Send DTMF to any member of the conference	conference <confname> dtmf <member_id>|all|last|non_moderator <digits>[/duration]
					// gw_call_execute_vcmd(c,0,"%d dtmf $member_id %s/%d",cl->room_num,key,dtmf_duration);
					gw_call_execute_vcmd(c,0,"%d dtmf $member_id %s",cl->room_num,key);

					// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_uuid), SWITCH_LOG_NOTICE,
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(cl->session_sn), SWITCH_LOG_NOTICE,
						"M2 Bridge DTMF Event %s to %s \n", key, c->session_sn/*c->call_name*/);
				}
			}
		}
	}		
}

static int gw_ev_dtmf_handler(switch_event_t *event,int argc ,void * argv[])
{
	//get member id
	switch_core_session_t *session =NULL;
	const char * uuid=switch_event_get_header(event, "Unique-ID");

	// // no log for refer

	session=switch_core_session_locate(uuid);
	if(session)
	{
		const char * call_id;
		gw_call_leg_t * cl=NULL;
		switch_channel_t *channel = switch_core_session_get_channel(session);
		
		call_id = switch_channel_get_variable(channel,"gw_call_id");
		if(call_id){
			int dtmf_duration; 

			// "Action":"dtmf" ¤º¥u¦³ "DTMF-Key";"7", ¦Ó¨S¦³ "DTMF-Source" & "DTMF-Duration"

			const char * key=switch_event_get_header(event,"DTMF-Key");
			// const char * dtmf_source =switch_event_get_header(event,"DTMF-Source");				// event ¤º¨S¦³¦¹­È, ¦]¬°¦b conference_loop.c: -> conference_loop_event() ¨S¶ñ¤J
			const char * str_dtmf_duration = switch_event_get_header(event,"DTMF-Duration");	// event ¤º¨S¦³¦¹­È, ¦]¬°¦b conference_loop.c: -> conference_loop_event() ¨S¶ñ¤J

	//		const char * dtmf_source = NULL;
			const char *dtmf_source = switch_channel_get_variable(channel,"dtmf_source");  // ¤p¤ß, ­Y dtmf_source ¤º®e¦³»~«h«öÁä¥i¯à·|³Q¥á±ó (¦]¬°¤U¦C§PÂ_¦¡ªºÃö«Y)

	//		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "DTMF-Key = %s\n", key);
	//		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "DTMF-Source = %s\n", dtmf_source);
	//		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "DTMF-Duration = %s\n", str_dtmf_duration);

	//		dtmf_source = "RTP"; peter ¼W¥[¤Î mark

			// int dtmf_duration = str_dtmf_duration ? atoi(str_dtmf_duration):-1;	// peter 2019/3/13
			dtmf_duration = str_dtmf_duration ? atoi(str_dtmf_duration):400;	// is null
			cl = gw_find_callleg(call_id);
			// FIXBUG MII-60

#ifdef MII60			
			
			if( cl->dtmf_type == SWITCH_DTMF_RTP  && dtmf_source && strcmp(dtmf_source,"RTP") )
			{
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG,
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
					"M2 %s Abandon User DTMF Event Inband :%s\n",cl?cl->call_name:"NULL",key);
				switch_core_session_rwunlock(session);
				return GW_RETURN_OK;
			}

			if( cl->dtmf_type == SWITCH_DTMF_INBAND_AUDIO  && dtmf_source && strcmp(dtmf_source,"RTP")==0 )
			{
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG,
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
					"M2 %s Abandon User DTMF Event rfc2833 :%s\n",cl?cl->call_name:"NULL",key);
				switch_core_session_rwunlock(session);
				return GW_RETURN_OK;
			}
			
#endif 
			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_NOTICE,

			// 2020/9/1 ¥[¤J§PÂ_ DTMF¬O§_¬°´Ý¯d key
			// MII-159
			if(cl->dtmf_ignore_ms >0)
			{
				switch_time_t now_ts;

				now_ts = switch_time_ref();

				if(cl->dtmf_last[0] == key[0])
				{
					if(now_ts - cl->dtmf_last_ts < cl->dtmf_ignore_ms)	// ´Ý¯d key
					{
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
							"User DTMF Event:%s DTMF Source:%s DTMF Type:%s  (ignore)\n",key,
							dtmf_source?dtmf_source:"Unknown",cl->dtmf_type == SWITCH_DTMF_RTP?"RTP":"Inband");
						switch_core_session_rwunlock(session);
						return GW_RETURN_OK;
					}
				}
				cl->dtmf_last[0] = key[0];
				cl->dtmf_last_ts = now_ts;
			}

			if(cl->Security)
			{
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
					"User DTMF Event:(X) DTMF Source:%s DTMF Type:%s\n",
					dtmf_source?dtmf_source:"Unknown",cl->dtmf_type == SWITCH_DTMF_RTP?"RTP":"Inband");
			}
			else
			{
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO,
					"User DTMF Event:%s DTMF Source:%s DTMF Type:%s\n",key,
					dtmf_source?dtmf_source:"Unknown",cl->dtmf_type == SWITCH_DTMF_RTP?"RTP":"Inband");
			}
			
			if(cl){
				gw_call_leg_dtmf_handler(cl,key,dtmf_duration);
			}else{
				// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG,
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
					"M2 %s Call leg is not found\n",call_id);
			}
		}else{
			// switch_log_printf(SWITCH_CHANNEL_UUID_LOG(uuid), SWITCH_LOG_DEBUG,
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
				"M2 %s Call leg id is not found\n",call_id);
		}
		switch_core_session_rwunlock(session);
	}
	else
	{
		// gw_event_dump(event);
		// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "M2 DTMF EV Can Not Found Unique-ID  \n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "M2 DTMF EV Can Not Found Unique-ID  \n");
	}

	return GW_RETURN_OK;
}


//switch_status_t mod_gw_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool)
SWITCH_MODULE_LOAD_FUNCTION(mod_gw_load)
{
	int i;
	switch_api_interface_t *gw_api_interface;
	switch_application_interface_t *app_interface;
	char *fax_enable;
	
	// Number 1
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, GW_HELLOWORLD);    
	{
		//write version informations 
		FILE * fp = fopen("/mmcc/version/gw.version","w");
		if(fp){
			fprintf(fp,"gw %s\n",GW_VERSION);
			fclose(fp);
		}
	}

    memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	globals.max_threads=120;
	globals.num_threads=0;

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.callegs_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.recording_mutex, SWITCH_MUTEX_NESTED, globals.pool);	// peter
	
	switch_core_hash_init(&globals.callegs_hash);
	switch_core_hash_init(&globals.callegs_timeout_hash);
	switch_core_hash_init(&globals.recording_hash);		// peter

	switch_queue_create(&globals.event_queue, GW_EVENT_QUEUE_SIZE, globals.pool);
	switch_queue_create(&globals.event_retry_queue, GW_EVENT_QUEUE_SIZE, globals.pool);
	switch_queue_create(&globals.event_tmp_queue, GW_EVENT_QUEUE_SIZE, globals.pool);

	switch_core_hash_init(&globals.room_hash);
	switch_queue_create(&globals.room_queue, 8192, globals.pool);

	switch_mutex_lock(globals.mutex);
	globals.running = 1;
	globals.next_room_num=6000;
	switch_mutex_unlock(globals.mutex);

	gw_fill_room(2048);

	globals.debug=1;
	// MII-55 : support HA 
#ifdef  MII55

	globals.ha_argc = 0;
	for(i=0; i<MAX_HA_COUNT; i++)
	{
		globals.ha_argv[i] = NULL;
		globals.ha_time1[i] = 0;
		globals.ha_time2[i] = 0;
	}

	globals.sendingfax = 0;
#endif 

	// ¯u¥¿ÄdºI event ªº¬O¦b³o¸Ì, ¦Ó¤£¬O¦b¤U­±ªº bind_eventproc
	if (switch_event_bind(modname, SWITCH_EVENT_NOTIFY_IN, SWITCH_EVENT_SUBCLASS_ANY, gw_event_handler, NULL)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind NOTIFY IN EVENT!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_CONFERENCE, gw_event_handler, NULL)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind CONFERENCE EVENT! \n");
		return SWITCH_STATUS_GENERR;
	}

	if (switch_event_bind(modname, SWITCH_EVENT_CHANNEL_HANGUP,SWITCH_EVENT_SUBCLASS_ANY , gw_event_handler, NULL)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind HANGUP EVENT! \n");
		return SWITCH_STATUS_GENERR;
	}
	
	if (switch_event_bind(modname, SWITCH_EVENT_CUSTOM,"sofia::reinvite" , gw_event_handler, NULL)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind sofia::reinvite! \n");
		return SWITCH_STATUS_GENERR;
	}

	// user define => static void bind_eventproc(int id, const char * sub, const char * k,const char *v, eventproc_func_t handler)

	bind_eventproc(SWITCH_EVENT_CHANNEL_HANGUP, SWITCH_EVENT_SUBCLASS_ANY,NULL,NULL,gw_ev_hangup_handler);

	// Receiving a NOTIFY message will trigger gw_ev_notify_in_handler
	bind_eventproc(SWITCH_EVENT_NOTIFY_IN, SWITCH_EVENT_SUBCLASS_ANY,NULL,NULL,gw_ev_notify_in_handler);

	// #define SWITCH_EVENT_SUBCLASS_CONFERENCE "conference::maintenance"
	// #define SWITCH_EVENT_SUBCLASS_GW "gw::control"

	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","IVR_PLAY_PROMPT_REQ",gw_ev_play_prompt_handler);
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","MERGE_CALL_REQ",gw_ev_merge_call_handler);
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","UNMERGE_CALL_REQ",gw_ev_unmerge_call_handler);	// 2023/7/14
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","RECORD_CALL_REQ",gw_ev_record_call_handler);	// peter
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","RECORD_STOP_REQ",gw_ev_record_stop_handler);	// peter
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","RECORD_PARTY_REQ",gw_ev_record_party_handler);	// peter 2019/12/3

	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","DETECT_DTMF_START_REQ",gw_ev_detect_dtmf_handler);	// peter 2022/6/10

	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","HANGUP_IVR_REQ",gw_ev_hangup_ivr_handler);
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","MONITOR_CALL_REQ",gw_ev_monitor_call_handler);
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","COACH_CALL_REQ",gw_ev_coach_call_handler);
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","ADJUST_VOLUME_REQ",gw_ev_adjust_volume_call_handler);
#ifdef MII103
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","MUTE_CALL_REQ",gw_ev_mute_call_call_handler);
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_GW,"Action","UNMUTE_CALL_REQ",gw_ev_unmute_call_call_handler);	
#endif 
	//
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_CONFERENCE,"Action","add-member",gw_ev_add_member_handler);
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_CONFERENCE,"Action","del-member",gw_ev_del_member_handler);

	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_CONFERENCE,"Action","conference-destroy",gw_ev_conference_destroy_handler);
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_CONFERENCE,"Action","play-file-done",gw_ev_conf_play_file_done_handler);
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_CONFERENCE,"Action","play-file-member-done",gw_ev_play_file_done_handler);
	
	bind_eventproc( SWITCH_EVENT_CUSTOM, SWITCH_EVENT_SUBCLASS_CONFERENCE,"Action","dtmf",gw_ev_dtmf_handler);

	bind_eventproc( SWITCH_EVENT_CUSTOM, "sofia::reinvite",NULL,NULL,gw_ev_reinvite_handler);

	SWITCH_ADD_APP(app_interface, "gw", GW_DESC, GW_DESC, gw_function, GW_USAGE, SAF_NONE);

	SWITCH_ADD_API(gw_api_interface, "gw", GW_DESC, gw_api_debug_function, GW_SYNTAX);

	send_oam_notify("000", "", "");
	/* indicate that the module should continue to be loaded */

	return SWITCH_STATUS_SUCCESS;        
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_gw_shutdown)
{
	int sanity=0;
	switch_hash_index_t *hi = NULL;
	int i;

	send_oam_notify("099", "", "");

	switch_mutex_lock(globals.mutex);
	if (globals.running == 1) {
		globals.running = 0;
	}
	switch_mutex_unlock(globals.mutex);

//	switch_event_free_subclass(VM_EVENT_MAINT);
	switch_event_unbind_callback(gw_event_handler);

	
	while (globals.num_threads) {
		switch_cond_next();
		if (++sanity >= 60000) {
			break;
		}
	}

	for(i=0; i<MAX_HA_COUNT; i++)	// free
	{
		if(globals.ha_argv[i] != NULL)
			free(globals.ha_argv[i]);
	}


	// peter
	switch_mutex_lock(globals.recording_mutex);
	
	while ((hi = switch_core_hash_first_iter( globals.recording_hash, hi))) {
		void *val = NULL;
		const void *key;
		switch_ssize_t keylen;
		gw_recording_t *rec;
		
		switch_core_hash_this(hi, &key, &keylen, &val);
		rec = (gw_recording_t *) val;

		switch_core_hash_delete(globals.recording_hash, rec->RecordFile);
	}
	switch_mutex_unlock(globals.recording_mutex);
	//


	switch_mutex_lock(globals.callegs_mutex);
	
	while ((hi = switch_core_hash_first_iter( globals.callegs_hash, hi))) {
		void *val = NULL;
		const void *key;
		switch_ssize_t keylen;
		gw_call_leg_t* cl;
		
		switch_core_hash_this(hi, &key, &keylen, &val);
		cl = (gw_call_leg_t *) val;

		switch_core_hash_delete(globals.callegs_hash, cl->call_id);
		switch_core_hash_delete(globals.callegs_timeout_hash, cl->call_id);

		switch_core_destroy_memory_pool(&cl->pool);
		
	}

//	switch_core_hash_destroy(&globals.profile_hash);

	switch_mutex_unlock(globals.callegs_mutex);

	switch_mutex_destroy(globals.callegs_mutex);
	switch_mutex_destroy(globals.mutex);

	switch_mutex_destroy(globals.recording_mutex);	// peter

	return SWITCH_STATUS_SUCCESS;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */


void plog(const char *fmt, ...)
{
	/*
	FILE *fp;
	va_list pArgList;
	char *data;
	int ret;

	va_start(pArgList, fmt);

	ret = switch_vasprintf(&data, fmt, pArgList);

	va_end(pArgList);

	if(ret == -1)
		return;

	if(strcmp(oldmsg, data) == 0)
		return;

	sprintf(oldmsg, "%s", data);

	fp = fopen("/var/opt/mmcc/gw/log/log.txt", "a+t");
	if(fp != NULL)
	{
		fprintf(fp, "%s", data);
		fclose(fp);
	}

	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[PLOG] %s", data);

	switch_safe_free(data)
	*/

	/*
	·í¦¬¨ì
	(1)
	OPTIONS sip:ping@172.16.0.92:5070 SIP/2.0
	Call-ID: 8aba799c4ccf3d0e07669d4bf752ddf3@172.16.0.92
	CSeq: 1 OPTIONS
	From: <sip:ping@172.16.0.92:5070>;tag=8da0f7b7-11792081
	To: <sip:ping@172.16.0.92:5070>
	Via: SIP/2.0/UDP 172.16.0.92:5060;branch=8da0f7b7-11792081-8aba799c4ccf3d0e07669d4bf752ddf3-172.16.0.92-1-options-172.16.0.92-5060343733
	Max-Forwards: 70
	Contact: <sip:172.16.0.92:5060>
	Content-Length: 0

	·|Ä²µo :
	gw_event_handler
    switch_queue_trypop() SWITCH_STATUS_SUCCESS 
    actual_message_query_handler
    gw_ev_notify_in_handler
    gw_timeout_run
	*/

	/*
	(2) INVITE ¤£·|ª½±µÄ²µo gw_event_handler, ¦Ó·|Ä²µo
	SWITCH_STANDARD_APP(gw_function)
	gw_pop_room
		switch_core_hash_insert Call-ID:"6f8d28-7e0010ac-13c4-55013-5c63d5a2-3a1cf9f0-5c63d5a2"
		switch_core_session_execute_application conference_num:"6000@gw"
	gw_event_handler
		switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
		switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_ev_add_member_handler
	gw_find_room_by_event
	gw_find_callleg
	gw_call_execute_wait_cmds
	gw_retry_ev
	gw_timeout_run
	gw_event_handler
		switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_timeout_run

             : (loop)

	gw_event_handler
	switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_timeout_run
	*/

	/*
	(3) ¦¬¨ì NOTIFY
	gw_event_handler
	switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_ev_notify_in_handle
*		show body
	actual_message_query_handler
	gw_ev_play_prompt_handler
	gw_find_callleg
	cJSON_GetObjectInt
			:
	gw_get_room_member_count
	gw_call_leg_stop_play
	gw_call_leg_play
	gw_call_execute_vcmd
	gw_conference_execute_cmd

	gw_event_handler
	gw_timeout_run
		switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_timeout_run

	gw_event_handler
	switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_ev_dtmf_handler
	gw_find_callleg
	gw_call_leg_dtmf_handler
	gw_call_leg_stop_play
	gw_call_execute_vcmd
	gw_conference_execute_cmd
	gw_call_leg_start_timer
	gw_report_dtmf_event
	gw_report_dtmf_event_
	gw_call_leg_stop_play
	gw_play_code_to_str
	gw_timeout_run
*		send IVR_PLAY_PROMPT_EVT
	gw_event_handler
	switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_ev_play_file_done_handler
	gw_find_callleg
	gw_timeout_run

	¦¬¨ì¤U¤@­Ó NOTIFY event
	gw_event_handler
	switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_ev_notify_in_handle
*		show body
	actual_message_query_handler
	gw_ev_play_prompt_handler
	gw_find_callleg
	cJSON_GetObjectInt
			:
	gw_get_room_member_count
	gw_call_leg_stop_play
	gw_call_leg_play
	gw_call_execute_vcmd
	gw_conference_execute_cmd

	gw_event_handler
	gw_timeout_run
		switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_timeout_run

	¦¬¨ì BYE
	gw_event_handler
	gw_event_handler
		switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_ev_hangup_handler
	gw_find_callleg
	gw_find_room
	gw_rm_callleg
	switch_queue_trypop() SWITCH_STATUS_SUCCESS
*		hanging up, cause: NORMAL_CLEARING
	actual_message_query_handler
	gw_ev_play_file_done_handler
	gw_event_dump			// ¬°¦ó gw_ev_play_file_done_handler «á·|¦³ gw_event_dump
	switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
		switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_find_room_by_event
	gw_ev_del_member_handler
	gw_timeout_run

	mod_conference.c:782 Write Lock ON
	mod_conference.c:785 Write Lock OFF
	gw_event_handler
		switch_queue_trypop() SWITCH_STATUS_SUCCESS
	actual_message_query_handler
	gw_find_room_by_event
	gw_ev_conference_destroy_handler

	mod_gw.c:2226 iHMP Push back Room:6000 
	gw_push_room
	gw_timeout_run
	*/
}
