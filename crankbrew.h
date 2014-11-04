#define TEMP_UPDATE_EVENT "temp_update"
#define TEMP_UPDATE_FMT "4f1 temp1 4f1 temp2"

typedef struct temp_update {
	float	temp1;
	float 	temp2;
} temp_update_t;

typedef struct _thdata {
	gre_io_t *send_handle;
	temp_update_t temp_data;
} thdata;


