#define TEMP_UPDATE_EVENT "temp_update"
#define TEMP_UPDATE_FMT "4f1 temp1 4f1 temp2"
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024*(EVENT_SIZE+16))

typedef struct temp_update {
	float	temp1;
	float 	temp2;
	float set_point;
} temp_update_t;

typedef struct _thdata {
	gre_io_t *send_handle;
	temp_update_t temp_data;
	int good_read;
	int heatcool;
	int read_freq;
	int debug;
} thdata;


