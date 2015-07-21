/* main.c -- A one wire temperature monitoring system for
 * the Beaglebone Black
 *
 * This program monitors a couple of DS18B20 one wire temp probes
 * and relays this information to a Storyboard UI.
 *
 * Copyright (C) 2014 Deric Panet-Raymond
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <gre/greio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>   
#include <errno.h>     
#include <string.h>  
#include <math.h>
#include "curl/curl.h"
#include "lcfg/lcfg.h"
#include "crankbrew.h"
#include <fcntl.h>
#include <dirent.h>
#include <syslog.h>

char * temp_wort;
char * temp_ambient;
char * web_address;
char *heat_control = "/sys/class/gpio/gpio51/value";
char *cool_control = "/sys/class/gpio/gpio60/value";
int cool_fd;
int heat_fd;
float setpoint = 16.0f;

struct MemoryStruct {
	  char *memory;
	  size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	  size_t realsize = size * nmemb;
	  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
		 
	  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	  if(mem->memory == NULL) {
		  /* out of memory! */ 
		  syslog(LOG_ERR,"not enough memory (realloc returned NULL)\n");
		  return 0;
	  }
							    
	  memcpy(&(mem->memory[mem->size]), contents, realsize);
	  mem->size += realsize;
	  mem->memory[mem->size] = 0;

	  return realsize;
}

int read_setpoint(void *data)
{
	thdata	*thread_data = (thdata *)data;
}
	
int log_temp(void *data) 
{
	thdata 			*thread_data = (thdata *)data;

	CURL *curl;
	CURLcode res;
	char postdata[255];

	struct MemoryStruct chunk;

	chunk.memory = malloc(1);
	chunk.size = 0;

	while (1) {

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (curl && thread_data->good_read == 1) {
			sprintf(postdata,"ambient=%f&wort=%f&sched=%d&heatcool=%d",thread_data->temp_data.temp2,thread_data->temp_data.temp1,2, thread_data->heatcool);
			curl_easy_setopt(curl,CURLOPT_URL, web_address);
			curl_easy_setopt(curl,CURLOPT_POSTFIELDS, postdata);
			if ( thread_data->debug == 1 ) {
				curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
			} else {
				curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
			}
			curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L); 
			res = curl_easy_perform(curl);
			if ( res != CURLE_OK) {
				fprintf(stderr, "curl_easy_perform() failed : %s\n", curl_easy_strerror(res));
			}
			thread_data->good_read = 0;
		}
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		sprintf(postdata,"");
		sleep(60);
	}
	return 0;
}

int temp_update(gre_io_t *send_handle, char * target, temp_update_t *event_data)
{
	gre_io_serialized_data_t    *nbuffer = NULL;
	int ret;

	/*
	 * Send a named event with an additional structure payload
	 */
	nbuffer = gre_io_serialize(NULL, target,
				TEMP_UPDATE_EVENT,
				TEMP_UPDATE_FMT,
				 event_data,
				 sizeof(temp_update_t));

	ret = gre_io_send(send_handle, nbuffer);
	//Free the allocated memory buffer for the serialized data
    gre_io_free_buffer(nbuffer);
    
	return ret;
}

int control_temp(char *heat,char *chill)
{
	int ret;
	int ret_1;
    ret = write(heat_fd,heat,1);
	ret_1 = write(cool_fd,chill,1);
	return ret && ret_1;
}

float read_temp(int fp) 
{
	char 			tmpData[6];
	unsigned char 	onewire[256];
	float 			rounded_down;
	int				num_read = 0;
	int				failed_read = 0;	
	float 			float_val = 0.0;
	memset(onewire,0,256);
	memset(tmpData,0,6);	
	
	while((num_read = read(fp, onewire, 256)) > 0) 
	{
		if ( strstr(onewire, "NO") == NULL ) {
			strncpy(tmpData, strstr(onewire, "t=") + 2, 5); 
			float_val = strtof(tmpData, NULL);
			float_val = float_val /1000;
			syslog(LOG_INFO,"Read Temp: %.3f C\n", float_val);
			failed_read = 0;
		} else {
			failed_read = 1;
			break;
		}
	}
	if ( failed_read == 1 ) {
		syslog(LOG_ERR, "Failed to read Wort temp sensor, skipping to next cycle");
		return -999.0;
	}
	rounded_down = ceilf(float_val * 100) / 100;
	if (rounded_down > 100.0f) {
		return -999.0;
	}
	return rounded_down;
}


static void * check_temperature(void * data) 
{
	thdata 			*thread_data = (thdata *)data;
	temp_update_t 	event_data;
	int 			ret;
	float 			old_ambient,old_wort;
	int 			fp, fp1;
	int 			a,b;
	char 			hex_string[6];
	int 			dec_val = 0;
	float 			temp_read;
	while (1) 
	{
		fp = open(temp_wort, O_RDONLY);
		fp1 = open(temp_ambient, O_RDONLY);
		if ( fp != -1 && fp1 != -1 )
		{
			old_wort = event_data.temp1;
			old_ambient = event_data.temp2;
			
			// read wort then read ambient
			temp_read = read_temp(fp);
			if ( temp_read != -999.0 ) {
				event_data.temp1 = temp_read;
			} else {
				event_data.temp1 = old_wort; 
				syslog(LOG_ERR, "one wire read failed for Wort, using old temp of %f",event_data.temp1);
			}
			temp_read = read_temp(fp1);
			if ( temp_read != -999.0 ) {
				event_data.temp2 = temp_read;
				thread_data->good_read = 1;
			} else {
				event_data.temp2 = old_ambient;
				syslog(LOG_ERR, "one wire read failed for Ambient, using old temp of %f",event_data.temp2);
			}
			if ( event_data.temp1 != old_wort || event_data.temp2 != old_ambient ) {
				thread_data->temp_data.temp1 = event_data.temp1;
				thread_data->temp_data.temp2 = event_data.temp2;
				syslog(LOG_DEBUG,"GOT WORT TEMP OF : %f and AMBIENT OF %f and heatcool is %d", thread_data->temp_data.temp1,thread_data->temp_data.temp2,thread_data->heatcool);
			//	ret = temp_update(thread_data->send_handle, NULL, &event_data);
			}
		
			if ( thread_data->temp_data.temp1 < (setpoint-0.3) && thread_data->temp_data.temp1 != -1 ) {
				syslog(LOG_NOTICE,"Heat Enabled [ Setpoint %f : Wort Temp : %f\n", setpoint,thread_data->temp_data.temp1);
				if ( control_temp("1","0") ) {
					thread_data->heatcool=2;
				} else {
					syslog(LOG_ERR,"Error, unable to enable heating");
				}
			}
			if ( thread_data->temp_data.temp1 == setpoint || thread_data->temp_data.temp1 == (setpoint+0.1) || thread_data->temp_data.temp1 == (setpoint-0.1) ) {
				syslog(LOG_NOTICE,"Heating and Cooling disabled");
				if ( control_temp("0","0") ) {
					thread_data->heatcool=1;
				} else {
					syslog(LOG_ERR,"Unable to disable heating and cooling\n");
				}
			}
			if ( thread_data->temp_data.temp1 > (setpoint + 0.4)  && thread_data->temp_data.temp2 >= 3 && thread_data->temp_data.temp1 != -1) {
				syslog(LOG_NOTICE,"Chill Enabled [ Setpoint %f : Wort Temp : %f\n", setpoint,thread_data->temp_data.temp1);
				if ( control_temp("0","1") ) {
					thread_data->heatcool=3;
				} else {
					syslog(LOG_ERR,"Unable to enable the chiller\n");
				}
			}
			close(fp);
			close(fp1);
			sleep(thread_data->read_freq);
		 } else {
			syslog(LOG_ERR, "unable to open temperature sensors");
		}
	}
}


enum lcfg_status read_cfg(const char *key, void *data, size_t len, void *user_data) {
    int i;
    char c;

    printf("%s = \"", key);
    for( i = 0; i < len; i++ ) {
        c = *((const char *)(data + i));
        printf("%c", isprint(c) ? c : '.');
    }
    puts("\"");

    return lcfg_status_ok;
}

int main(int argc, char* argv[])
{
	pthread_t temperature_thread,templog_thread;
	int not_connect = 0;
	int c;
	char *channel = NULL;
	pthread_mutex_t mutex;
	pthread_cond_t condvar;
	int ret;
	int done = 0;
	gre_io_t *send_handle;
	thdata data1;
	size_t len;
	struct lcfg *cfg = lcfg_new("./brew.cfg");
	
	//open the log
	openlog("Fermtroller", LOG_PID, LOG_USER);

	if ( lcfg_parse(cfg) != lcfg_status_ok ) {
		syslog(LOG_INFO,"Error reading config file : %s\n", lcfg_error_get(cfg));
	} else {
		lcfg_accept(cfg, read_cfg,0);
	}

	lcfg_value_get(cfg, "wort_address",(void *)&temp_wort, &len);
	lcfg_value_get(cfg, "ambient_address",(void *)&temp_ambient, &len);
	if ( lcfg_value_get(cfg, "web_address", (void *)&web_address, &len) != lcfg_status_ok ) {
		syslog(LOG_ERR,"Error reading web_address : %s\n", lcfg_error_get(cfg));
	}

	// assign the GPIO file descriptors
	
 	cool_fd = open(cool_control,O_WRONLY);
	heat_fd = open(heat_control,O_WRONLY);

	data1.temp_data.temp1 = 0.0;
	data1.temp_data.temp2 = 0.0;
	data1.good_read = 0;	
	data1.heatcool = 1;
	data1.debug = 0;
	data1.read_freq = 30; // default read frequency is 30 seconds
	control_temp("0","0");
	while ((c = getopt( argc, argv, "vc:p:t:r:")) != -1 )
	{
		switch(c)
		{
			case 'v':
				data1.debug = 1;
				break;
			case 'c':
				channel = optarg;
				syslog(LOG_INFO,"Setting channel to : %s\n", channel);
				break;
			case 't':
				setpoint= atof(optarg);
				syslog(LOG_INFO,"Setting Ferment temp to %f\n", setpoint);
				break;
			case 'r':
				data1.read_freq = atoi(optarg);
				syslog(LOG_INFO,"Setting read frequency to %d\n", data1.read_freq);
				break;

		}
	}
#ifdef STORYBOARD
	if ( channel == NULL ) {
		syslog(LOG_ERR,"You must supply a greio channel temp_driver -c <storyboard channel>\n");
		exit(-1);
	}
	send_handle = gre_io_open(channel, GRE_IO_TYPE_WRONLY);
	if(send_handle == NULL && channel != NULL ) {
		syslog(LOG_ERR,"Can't open send handle\n");
		while ( not_connect == 0 ) {
			send_handle = gre_io_open(channel, GRE_IO_TYPE_WRONLY);
			if ( send_handle != NULL ) {
				not_connect = 1;
			}
			syslog(LOG_INFO,"Waiting to connect\n");
			sleep(1);
		}
	}

	// push the send handle into the thread data pointer so we can access it in our check_tmep thread.
	data1.send_handle = send_handle;
#endif
	syslog(LOG_INFO,"Fermtroller starting");
	pthread_create(&temperature_thread, NULL, (void *)&check_temperature, &data1);
	sleep(1);
	pthread_create(&templog_thread, NULL, (void *)&log_temp, &data1);
	ret = pthread_mutex_init(&mutex, NULL);
	if(ret != 0) {
		syslog(LOG_ERR,"Mutex error\n");
		return -1;
	}

	ret = pthread_cond_init(&condvar, NULL);
	if(ret != 0) {
		syslog(LOG_ERR,"Condvar error\n");
		return -1;
	}
	
	//get_input(&data1);
	while(!done) {
		syslog(LOG_INFO,"waiting\n");
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&condvar, &mutex);
	}


	lcfg_delete(cfg);
	free (temp_wort);
	free (temp_ambient);
	free (web_address);
	syslog(LOG_INFO,"Fermtroller shutting down\n");

	close(cool_fd);
	close(heat_fd);
	closelog();
	return 0;
}
