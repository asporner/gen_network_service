
#ifndef _DEFINE_C4_SYS_H
#define _DEFINE_C4_SYS_H

#include <mysql.h>

#ifndef FreeBSD
#define OPENSSL_API_COMPAT 0x0908
#endif

#include <time.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/sem.h>
#include <stdio.h>
#include <unistd.h>

#include "c4_session.h"

#define	PATH_SERVER_LOG					"/tmp/core_service.log"
#define CORE_LIBEXEC                     "/home/core/libexec"
#define SHM_ID                      	0x0deaddea1
#define APP_CTX_SIZE					2048

#define CORE_SGET_WORKER(worker_id, we, is_ssl)                             \
    we = &shm_dbf->worker_tbl[is_ssl][worker_id];

#define LOCK_SEM_SESSION()													\
	c4_lock_ipc(CORE_SEM_SESSION)

#define UNLOCK_SEM_SESSION()												\
	c4_unlock_ipc(CORE_SEM_SESSION)

typedef enum {
        CORE_SEM_BATCH,
        CORE_SEM_WEB_ACCEPT,
        CORE_SEM_WEB_ACCEPT_SSL,
        CORE_SEM_WEB_ACCEPT2,
        CORE_SEM_EVENT,
        CORE_SEM_FORM,
        CORE_SEM_QUEUE,
        CORE_SEM_SESSION,
		CORE_SEM_GENERIC
} C4_SEM_ENUM;

/* ------------------------------------------------------- 	*/
/*															*/
/*				C L I E N T _ V A R S  _ T					*/
/*															*/
/*  This type contains all of the variables that are 		*/
/*  needed in the request context (as opposed to the 		*/
/*  session context).   									*/
/*															*/
/*	Members:												*/
/*		sock_fd				Request Socket File Desc.		*/			
/*		s					Pointer to session object.		*/
/*		domain_id			Domain ID of the site.			*/
/*		initial_block		Message delimiter flag			*/
/*		url					Request URL						*/
/*		post_in_ptr			POST request pointer			*/
/*		is_mobile			Flag to indicate mobile client	*/
/*		shm_dbf				Pointer to shared mem database	*/				
/*		dbh					pointer to database descriptor	*/
/*															*/
/* ------------------------------------------------------- 	*/
typedef struct client_vars {
    int     					sock_fd;
	MYSQL						*dbh;
	c4_shm_t					*shm_dbf;
    c4_session_t 				*s;
	SSL                         *ssl;
	int							w_id;
	char						*post_in_mem;
	char						*post_in_ptr;
	char						*post_out_mem;
	char						*post_out_ptr;

	char						h_name[128];
	char						uuid[64];
	int 						mobile_detected;	
	int							is_get;
	int 						is_mobile;	
	int							is_ssl;
	int 						is_touch;	
	int							initial_block;
	int							content_length;
	int							core2_selector;
	char						doc_root[512];
	char						app_ctx[APP_CTX_SIZE];
	char						get_parms[512];
	char						*argv[32];
	int							argc;

	/*
	 * Domain related data
 	 */

    int     					domain_id;
	char						domain_name[64];
	char						domain_url[64];
	int							domain_parent;
	int							domain_is_mobile_capable;
	int							domain_ssl_enable;
	int							domain_in_dft_cert;

} client_vars_t;

#define c4_outs(buff, format, args...)										\
		sprintf(buff, format, ## args);										\
		buff += strlen(buff);

void c4_db_quote_string (char *d, char *s);

void gen_tokens(char *str, char sep, int *argc, char **argv);

int log_printfx(char *file, int line, char *fmt, ...);

int c4_outf(struct client_vars *ce, char *fmt, ...);

int c4_call_rpc(client_vars_t *ce, char *n_rpc, void *parms);

int c4_gen_write(struct client_vars *ce, char *buff, int bsize);

int c4_gwrite(int fd, char *buff, int bsize);

int c4_lock_ipc(C4_SEM_ENUM sem);

int c4_unlock_ipc(C4_SEM_ENUM sem);

void c4_sigalarm_handler(int sig);

int c4_init_session_from_mac(client_vars_t *ce, char *mac);

int c4_pwstrength(client_vars_t *ce, char *passwd, int *flags);

int c4_getstr(char *buffstr, int maxlen, FILE *fp);

int c4_parse_attrib(char *buffstr, char *r_var, char *r_val);

char *c4_unquote_parm(char *param);

int  c4_process_conf_file(FILE *fp, void *parms, int (*fn)(  void *parms, char *var, char *val) );

#define log_printf(fmt, args...) log_printfx(__FILE__, __LINE__, fmt, ## args);

int c4_sys_init(c4_shm_t *shm);


#endif /* _DEFINE_C4_SYS_H */
