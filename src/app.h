#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <setjmp.h>
#include <math.h>
#include <mysql.h>
#include "hpdf.h"

#include "common.h"
#include <locale.h>

#define VX_CHAR						  0
#define VX_NUM						  1
#define VX_VOID						  2
#define VX_TIME						  3
#define MAX_REC						100
#define REPORT_INITIAL_X_POS         37

#define PATH_SW_PACKAGE             "/tmp/pkg.load.tar.gz"
#define STYLE_OVRD "background-color:white;color:black;border:2px solid black;"

#define IF_NAME                   "eno1"

#define PRINT_DFT_MALLOC_SIZE (64 * 1024)


#define SYS_CONFIG_ROOT				100
#define SYS_CONFIG_SETUP			200
#define SYS_DRAWER_TOTAL			300
#define SYS_CONFIG_MESG				400

#define CONFIG_OPTION				  0
#define CONFIG_MENU					  1
#define UNRESTRICT					  0
#define RESTRICT					  1
#define CORE_PDF_MAX_PAGES          200
#define CORE_PDF_MAX_INDEX          512

#define DISPLAY_MODE_REPORTS		  1
#define DISPLAY_MODE_CONFIG           2
#define DISPLAY_MODE_SYSTEM           3
#define DISPLAY_MODE_CAT_CONF         4		
#define DISPLAY_MODE_ITEM_CONF        5
#define DISPLAY_MODE_PREF_CONF        6
#define DISPLAY_MODE_STORES_CONF      7
#define DISPLAY_MODE_UNIT_CONF        8
#define DISPLAY_MODE_USER_CONF        9
#define DISPLAY_MODE_MENU_CONF       10 
#define DISPLAY_MODE_PRINTER_CONF    11
#define DISPLAY_MODE_RECEIPT_CONF    12
#define DISPLAY_MODE_RECIEVE_STOCK	 13 
#define DISPLAY_MODE_DRAWER_TOTAL    14
#define DISPLAY_MODE_TAB_CONFIG      15
#define DISPLAY_MODE_POS_RUN		 16
#define DISPLAY_MODE_POS_EDIT		 17
#define DISPLAY_MODE_POS_TABLE_ENTRY 18
#define DISPLAY_MODE_POS_PAYMENT	 19
#define DISPLAY_MODE_POS_DONE		 20 
#define DISPLAY_MODE_PRINTER_CONTROL 21
#define DISPLAY_MODE_MENU_ITEM_CONF	 22 
#define DISPLAY_MODE_POS_STORE_ENTRY 23
#define DISPLAY_MODE_REFUND          24

#define ENT  "&crarr;"
#define CLK  "&#8681;"
#define CUL  "<del>&#8681;</del>"

#define SHFT "&#8679;"
#define LAR  "&larr;"
#define BSL  "&#92;"
#define TAB  "&#8633"
#define QOT  "&quot"

#define CHECK_SESSION(s)                                                    \
    if ((ce->s == NULL) || (ce->s->sesn_state == 0)) {                      \
        return 400;                                                         \
    }


typedef enum {
							PDF_VAR_TYPE_CHAR               = 0,
							PDF_VAR_TYPE_INT,
							PDF_VAR_TYPE_SHORT,
							PDF_VAR_TYPE_LONG,
							PDF_VAR_TYPE_FLOAT,
							PDF_VAR_TYPE_DOUBLE
} pdf_vtypes_e;

 
typedef enum {
							PDF_DISP_TYPE_NUMBER            = 0,
							PDF_DISP_TYPE_CURRENCY,
							PDF_DISP_TYPE_STRING
} pdf_dtypes_e;

struct creg_pie_values {
    						double pct_val;
    						double raw_val;
    						char legend[100];
};

struct keypad_directory {
							char keypad_name[40];
							struct layout_table_fmt *keypad;
							int size;
};

struct rpc_table_entry {
    						char rpc_name[100];
    						int (*rpc_fn)(struct client_vars *ce);
};

struct key_config {
							char label[30];
							int x_size;
							int m_type;
							int cond;
							char parm[10];
							char col[30];
							int col_type;	
							int (*edit_fn)(struct client_vars *ce, int mode);
							char desc[100];
};

struct config_rec {
							struct key_config *rec;
							int rec_len;
							char db_name[60];
							char db_key_field[60];
							char db_insert_fn[60];
							char db_delete_fn[60];
							char menu[120];
};

struct config_preferences {
							char label[60];
							char col[40];
							int  col_type;
							int (*edit_fn)(struct client_vars *ce, int mode);
};

struct int_menu_config {
    						char label[100];
    						int x_size;
    						char col[60];
    						int col_type;
    						int (*edit_fn)(struct client_vars *ce, int mode);
};

#define PAY_CASH 			0
#define PAY_CHECK			1
#define PAY_CARD 			2
#define PAY_COMP 			3
#define PAY_TAB				4
#define PAY_VOID			5

typedef enum {
    						CREG_CONF_CATEGORY,
    						CREG_CONF_ITEMS,
    						CREG_CONF_STORES,
    						CREG_CONF_USERS,
    						CREG_CONF_MENU,
    						CREG_CONF_MENU_ITEM
} conf_type_t;

struct _cont {
	int						cont_id;			/* menu_id or icomp_id */
	int						cont_mode_create;	/* Set for create mode */
	int						cont_elem_id;
	int						cont_elem_item_id;
	int						cont_mode_elem_create;
};

struct _passwd {
	char					pw_1[30];
	char					pw_2[30];
	char					pw_status[60];

	int						login_phase;
	int						sub_idx;
	char					login_error[50];
	char					login_name[20];
	char					login_passwd[30];
};

struct _keyb {
	char					display_buffer[255];
	char					input_buffer[255];
	int						shift;
	int						shift_enable;
	int						lock;
	int						hide_chars;
	int						buffer_len;
	char					sound_fspec[64];
};

struct _time {
	unsigned char 			tm_hour;
	unsigned char			tm_tmin;
	unsigned char			tm_min;
	unsigned char			tm_pm;
};

struct _dl {
	char					dl_path[128];
	int						dl_phase;
	int						dl_ofs;
	int						dl_size;
	int						dl_current_segment;
	int						dl_local_vers;
	int						dl_segment_count;
	int						dl_updt_vers;
};

struct _pos {
	char					store_name[30];
	int						j_page;
	char 					table[64];
	int						pay_type;
	char					menu_name[60];
	int 					invoice;
	int						sys_tab_id;
	int						sys_is_misc;
	int						sys_tabs_open;
	int						sys_is_admin;
	int						sys_can_change_store;
	int						sys_is_superuser;
	int						sys_edit_item;
	int						sys_edit_option;
	int						sys_tab_credit_open;
	int						sys_payment_col;
	int						menu_id;
	int						menu_item;
	int						menu_option;
	int						menu_journal_id;
};

struct _cal {
	int						year;
	int						month;
	int						day;
	int						month_sel_open;
	int						year_sel_open;
};

struct _report {
	struct _cal				cal[2];
	char					end_date[12];
	char					start_date[12];
	int						sort_idx[2];
	int						sort_mode[2];
};

struct _receipt {
	int						line_size;
	int						char_width;
	int						justify_mode;
};

struct _cfg {
	int						sys_menu_init;
	int						sys_conf_row;
	int						sys_conf_col;
	int						sys_conf_sub_col;
	int						sub_idx;
	int						menu_parent;
	int						menu_depth;
	int						batch_id;
	int						buffer_dirty;

	struct key_config 		rec[MAX_REC];
	int 					rec_cnt;
	int						db_type;
	char					db_name[60];
	char					db_key_field[60];
	char					db_insert_fn[60];
	char					db_delete_fn[60];
	char					menu[60];
};

struct _updt {
	int						n_vers;
	int						o_vers;
	int						n_segments;
};

struct _refund {
	char					date[10];
	int						inv_id;
	int						journal_item;
	int						adj_qty[25];
};

struct _printers {
	int						pid;			/* Printer 		*/
	int						qid;			/* Queue entry 	*/
};

/*******************************************************************************
 *  \fn				Context Descriptor
 ******************************************************************************/
struct _posctx {
	int						sys_do_click;
	int						sesn_init;
	int						store_id;
	unsigned char			sys_display_mode;

	struct _refund			refund;
	struct _cont			cont;
	struct _passwd			passwd;
	struct _keyb			keyb;
	struct _time			time;
	struct _report			report;
	struct _dl				download;
	struct _cfg				cfg;
	struct _pos				pos;
	struct _receipt			rcpt;
	struct _printers		printers;
};

typedef struct layout_table_fmt {
    int						top;
    int						left;
    int						height;
    int						width;
    int						font_size;
	int						key;
    char					bg_rgb[16];
    char					fg_rgb[16];
    char					legend[100];
    char					js_fn[128];
} creg_keytable_t;

typedef struct {
	HPDF_Page				h;	
	int						pn;	
} creg_pdf_page_t;
	
typedef struct {
	HPDF_Doc				h;
	HPDF_REAL				height;
	HPDF_REAL				width;
	creg_pdf_page_t			*pages;
	creg_pdf_page_t			*current_page;
	char					doc_name[255];
	int						page_count;
	int						spage_num;
	int						c_page;
} creg_pdf_t;		

typedef enum {
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT
} creg_align_e;

struct creg_report_headers {
						    char col_name[80];
    						int y_pos;
							int width;
							creg_align_e align;
};

struct creg_report_config {
							int gt;
							int x;
							int y;
							int ln;
							int ht;
							int lc;
							int pn;
							creg_pdf_page_t *page;
							char desc[80];
};


/* Prototypes */

void gen_tokens(char *str, char sep, int *argc, char **argv);

void int_creg_get_pref_rgb_value(struct client_vars *ce, char *var, char *val);

int rpc_pos_gen_display(struct client_vars *ce);

void int_creg_drawer_totals(struct client_vars *ce);

double int_calculate_invoice_total(struct client_vars *ce, int invoice_id, int flag);

double int_calculate_tax_total(struct client_vars *ce);

int int_gen_new_invoice(struct client_vars *ce);

int int_creg_new_sale(struct client_vars *ce);

int int_get_invoice_item_count(struct client_vars *ce, int invoice_id);

int int_creg_set_jpage(struct client_vars *ce, int j_page);

void int_creg_reset_buffer(struct client_vars *ce);

int int_creg_sale_end(struct client_vars *ce);

int int_display_app_menu(struct client_vars *ce);

int int_touchscreen_login_page(struct client_vars *ce);

int int_calculate_subtotals(struct client_vars *ce);

int pos_food_drink_menu(struct client_vars *ce);

int pos_display_error(struct client_vars *ce);

int pos_edit_journal(struct client_vars *ce);

int int_creg_setup(struct client_vars *ce);

int pos_display_cmd_ribbon(struct client_vars *ce);

int pos_display_total(struct client_vars *ce);

void int_creg_reset_input(struct client_vars *ce);

int int_report_view(struct client_vars *ce);

int int_validate_login(struct client_vars *ce, char *user, char *passwd);

int int_creg_config_root(struct client_vars *ce);

int pos_misc_entry(struct client_vars *ce);

int creg_conf_raw_alpha_keyboard(struct client_vars *ce,
		int x_ofs, int y_ofs);

int int_creg_config_del(struct client_vars *ce);

int int_creg_conf_update_value(struct client_vars *ce, char *val);

int int_pos_gen_display(struct client_vars *ce);

int int_query_gen_display(struct client_vars *ce, char *sql);

int int_creg_pref_update_value(struct client_vars *ce, char *val);

int pos_void_sale(struct client_vars *ce);

int int_drawer_open(struct client_vars *ce);

int int_creg_drawer_total(struct client_vars *ce);



/* keypad.c */
int int_panel_button(struct client_vars *ce, int button_enable, 
		int xp, int yp, int xs, int ys, char *item, 
		char *js_fn, char *style_override);


/* report.c */

int int_creg_activity_report(struct client_vars *ce);

int rpc_creg_report_gen(struct client_vars *ce);

/* keypad.c */

int creg_panel_button(struct client_vars *ce, int top, int left, int height,
	int width, int font_size, char *bg_rgb, char *fg_rgb, 
	char *legend, char *js_fn, int key);

int creg_draw_key_layout(struct client_vars *ce, creg_keytable_t *ktbl, 
		int k_size, int x_ofs, int y_ofs);

/* pdf.c */

int creg_pdf_form_fill(creg_pdf_t *pdf, double r_color, double g_color,
						double b_color, double x_pos, double y_pos, 
						double width, double height);

creg_pdf_t *creg_create_pdf_document(char *doc_name);

int creg_pdf_left_text(creg_pdf_t *pdf, HPDF_Font font, int font_size, 
							int x_pos, int y_pos, char *text_buff);

int creg_pdf_center_text(creg_pdf_t *pdf, HPDF_Font font, int font_size,
							int y_pos, char *text_buff);

int creg_pdf_right_text(creg_pdf_t *pdf, HPDF_Font font, int font_size,
							int x_ofs, int y_pos, char *text_buff);

int creg_pdf_draw_hline(creg_pdf_t *pdf, int thickness, int x_from,
							int x_to, int y_pos);

int creg_pdf_draw_vline(creg_pdf_t *pdf, int thickness, int x_pos,
							int y_from, int y_to);

creg_pdf_page_t *creg_pdf_add_page(creg_pdf_t *pdf, int orient);

int creg_db_query_single_string(MYSQL *dbh, char *sql, char *res);

int creg_db_query_single_int(MYSQL *dbh, char *sql);

long creg_db_query_single_long(MYSQL *dbh, char *sql);

double creg_db_query_single_double(MYSQL *dbh, char *sql);

MYSQL_RES *int_sql_query_open(struct client_vars *ce, char *sql);

int creg_pdf_free(creg_pdf_t *pdf);

/* Inventory.c */

int int_creg_stock_receive(struct client_vars *ce);

int int_creg_setup_category(struct client_vars *ce);

int int_creg_setup_item_data(struct client_vars *ce);

int int_creg_setup_unit(struct client_vars *ce);

int int_creg_setup_preferences(struct client_vars *ce);

int int_creg_setup_stores(struct client_vars *ce);

int int_creg_setup_users(struct client_vars *ce);

int int_creg_load_user_config(struct client_vars *ce);

int int_creg_setup_receipt(struct client_vars *ce);

int int_creg_setup_printers(struct client_vars *ce);

int int_creg_setup_menus(struct client_vars *ce);

int int_creg_config_root(struct client_vars *ce);

int int_creg_printer_control(struct client_vars *ce);

int int_get_base_mac(char *buff);

int int_get_base_ip(char *buff, char *ifname);

int int_creg_setup_menu_item(struct client_vars *ce);

int int_pos_gen_display(struct client_vars *ce);

int int_pos_gen_pos_display(struct client_vars *ce);

int pos_item_lookup(struct client_vars *ce);

int pos_app_menu(struct client_vars *ce);

int pos_tab_conf(struct client_vars *ce);

int pos_table_button(struct client_vars *ce, int has_store);

int pos_display_journal(struct client_vars *ce, int invoice_id, int disp_mode);

int pos_apply_credit(struct client_vars *ce);

int int_panel_button(struct client_vars *ce, int button_enable, int xp, int yp,
			int xs, int ys, char *item, char *js_fn, char *style_override);

/* Keypad.c */
int creg_keypad_num_delete(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_num_keypad_m(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_num_keypad(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_alphakeys_upper_m(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_alphakeys_upper(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_alphakeys_lower_m(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_alphakeys_lower(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_enter_key(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_pref_number(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_conf_number(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_num2_keypad(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_num_keypad_m(struct client_vars *ce, int x_ofs, int y_ofs);

int creg_keypad_drawer_total_num_pad(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_login_layout_m(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_login_layout(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_login_layout_hide_m(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_login_layout_hide(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_table_enter_m(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_table_enter(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_recv_stock(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_endsale_m(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_endsale(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_misc_price_entry_m(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_misc_price_entry(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_ch_itm_qty_m(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_ch_itm_qty(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_ch_itm_price_m(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_ch_itm_price(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_update_conf_password(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_update_conf_enter(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_keypad_upd_cont_name(struct client_vars *ce, int x_ofs, int y_ofs);
int creg_tab_name_update(struct client_vars *ce, int x_ofs, int y_ofs);

int conf_alpha_keyboard(struct client_vars *ce, int mode);
int conf_password(struct client_vars *ce, int mode);
int conf_store(struct client_vars *ce, int mode);
int conf_tile_color(struct client_vars *ce, int mode);
int conf_on_off(struct client_vars *ce, int mode);
int conf_price_keyboard(struct client_vars *ce, int mode);
int conf_number_keyboard(struct client_vars *ce, int mode);
int conf_category(struct client_vars *ce, int mode);
int conf_taxable(struct client_vars *ce, int mode);
int conf_rights_edit(struct client_vars *cs, int mode);
int conf_bom(struct client_vars *cs, int mode);
int conf_menu(struct client_vars *cs, int mode);
int conf_units(struct client_vars *cs, int mode);
int conf_image(struct client_vars *cs, int mode);
int conf_time(struct client_vars *cs, int mode);
int conf_restrict(struct client_vars *cs, int mode);
int conf_print_type(struct client_vars *ce, int mode);
int conf_printer_select(struct client_vars *ce, int mode);
int conf_update_engine(struct client_vars *ce, char *sql);
void conf_load_ctx(struct client_vars *ce, struct config_rec *cfg);
int pos_queue_data(struct client_vars *ce, int doc_type, 
			char *print_ip_addr, char *content, char *options);

int int_get_reciept_print(struct client_vars *ce, char *addr);

int pos_store_button(struct client_vars *ce);

int int_report_headings(creg_pdf_t *pdf, creg_pdf_page_t *page, 
	struct creg_report_headers *header_table, int n_headers, int x_ofs);

int int_report_column_grids(creg_pdf_page_t *page, 
	struct creg_report_headers *header_table, int n_headers, int x_ofs);

int int_do_line_graph(struct client_vars *ce, char *run_date,
	creg_pdf_t *pdf, creg_pdf_page_t *page);

int int_do_summary_report(struct client_vars *ce, char *run_date,
	creg_pdf_t *pdf);

int int_do_detail_report(struct client_vars *ce, char *run_date,
	creg_pdf_t *pdf);

int int_do_stock_report(struct client_vars *ce, char *run_date,
	creg_pdf_t *pdf);

int creg_alpha_lock_off(struct client_vars *ce, int x_ofs, int y_ofs);

int creg_alpha_lock_on(struct client_vars *ce, int x_ofs, int y_ofs);

int creg_alpha_shift_off(struct client_vars *ce, int x_ofs, int y_ofs);

int creg_alpha_shift_on(struct client_vars *ce, int x_ofs, int y_ofs);

int int_do_pie_chart(struct client_vars *ce, char *run_date,
	creg_pdf_t *pdf, creg_pdf_page_t *page);

int int_is_journal_credit_entry(struct client_vars *ce);

int int_print_receipt(struct client_vars *ce);

int int_print_kitchen_pick(struct client_vars *ce);

int int_creg_print_drawer_totals(struct client_vars *ce);

double int_calculate_invoice_credits(struct client_vars *ce, int inv_id);

double int_calculate_tab_total(struct client_vars *ce, int tab_id);

int pos_refund(struct client_vars *ce);

int int_menu_panel_button(struct client_vars *ce, int button_enable,
		int xp, int yp, int xs, int ys, char *item, char *price, char *js_fn,
		char *style_override, char *image_path);

char  *int_common_receipt_print_init(struct client_vars *ce, 
		char *addr, int do_header);

creg_pdf_t *int_creg_report_init(struct client_vars *ce, 
					char *date_col, char *cond, char *sort);

int int_report_headers(struct client_vars *ce, creg_pdf_t *pdf, char *page_title);

int int_is_submenu(struct client_vars *ce, int menu_id);

int int_creg_report_inventory(struct client_vars *ce);

int int_creg_report_reorder(struct client_vars *ce);

int int_creg_report_ssummary(struct client_vars *ce);

int int_creg_report_sdetail(struct client_vars *ce);

int int_creg_report_detail_common(struct client_vars *ce, char *r_name,
	char *r_key, struct creg_report_headers *rhd, int len, 
	char *sql, char *xsql);

int int_update_receipt_message(struct client_vars *ce, char *mesg, int idx);
int app_call_rpc(struct client_vars *ce, char *rpc_name);
int rpc_creg_sys_login_phase_set(struct client_vars *ce);
int rpc_creg_post_touch_shift(struct client_vars *ce);
int rpc_creg_post_touch_unshift(struct client_vars *ce);
int rpc_creg_post_touch_lock(struct client_vars *ce);
int rpc_creg_post_touch_unlock(struct client_vars *ce);
int rpc_creg_post_touch_key(struct client_vars *ce);
int rpc_creg_post_touch_special_key(struct client_vars *ce);
int rpc_creg_post_touch_del_key(struct client_vars *ce);
int rpc_logout(struct client_vars *ce);
int rpc_login_post( struct client_vars *ce);
int rpc_creg_add_menu_item(struct client_vars *ce);
int rpc_creg_add_misc(struct client_vars *ce);
int rpc_creg_set_conf_db(struct client_vars *ce);
int rpc_creg_edit_item(struct client_vars *ce);
int rpc_creg_post_credit_inv(struct client_vars *ce);
int rpc_creg_post_credit_tab(struct client_vars *ce);
int rpc_creg_new_sale(struct client_vars *ce);
int rpc_creg_set_sesn_init(struct client_vars *ce);
int rpc_creg_logout(struct client_vars *ce);
int rpc_creg_set_store(struct client_vars *ce);
int rpc_creg_set_table(struct client_vars *ce);
int rpc_creg_set_jpage(struct client_vars *ce);
int rpc_creg_del_item(struct client_vars *ce);
int rpc_creg_del_user(struct client_vars *ce);
int rpc_creg_del_store(struct client_vars *ce);
int rpc_creg_update_qty(struct client_vars *ce);
int rpc_creg_update_price(struct client_vars *ce);
int rpc_creg_open_store_edit(struct client_vars *ce);
int rpc_creg_close_store_edit(struct client_vars *ce);
int rpc_creg_open_table_edit(struct client_vars *ce);
int rpc_creg_close_table_edit(struct client_vars *ce);
int rpc_creg_mesg_ack(struct client_vars *ce);
int rpc_pos_set_drawer_total(struct client_vars *ce);
int rpc_creg_apply_credit(struct client_vars *ce);
int rpc_creg_set_misc_mode(struct client_vars *ce);
int rpc_creg_set_display_mode(struct client_vars *ce);
int rpc_creg_set_conf_row(struct client_vars *ce);
int rpc_creg_set_conf_col(struct client_vars *ce);
int rpc_creg_set_conf_sub_col(struct client_vars *ce);
int rpc_creg_report_view_close(struct client_vars *ce);
int rpc_creg_conf_update_value(struct client_vars *ce);
int rpc_creg_pref_update_value(struct client_vars *ce);
int rpc_creg_new_category(struct client_vars *ce);
int rpc_creg_new_unit(struct client_vars *ce);
int rpc_creg_new_item(struct client_vars *ce);
int rpc_creg_new_store(struct client_vars *ce);
int rpc_creg_new_user(struct client_vars *ce);
int rpc_creg_new_menu(struct client_vars *ce);
int rpc_creg_new_menu_item(struct client_vars *ce);
int rpc_creg_new_menu_option(struct client_vars *ce);
int rpc_creg_del_menu(struct client_vars *ce);
int rpc_creg_del_unit(struct client_vars *ce);
int rpc_creg_new_group(struct client_vars *ce);
int rpc_creg_config_dright(struct client_vars *ce);
int rpc_creg_config_cright(struct client_vars *ce);
int rpc_creg_update_conf_password(struct client_vars *ce);
int rpc_creg_conf_pw_sfield(struct client_vars *ce);
int rpc_creg_set_conf_passwd(struct client_vars *ce);
int rpc_pos_kitchen_display(struct client_vars *ce);
int rpc_creg_close_inv_item(struct client_vars *ce);
int rpc_creg_set_conf_tile_color(struct client_vars *ce);
int rpc_creg_set_pref_tile_color(struct client_vars *ce);
int rpc_pos_gen_display(struct client_vars *ce);
int rpc_creg_reports(struct client_vars *ce);
int rpc_creg_report_gen(struct client_vars *ce);
int rpc_creg_void_sale(struct client_vars *ce);
int rpc_creg_drawer_open(struct client_vars *ce);
int rpc_creg_adj_stock(struct client_vars *ce);
int rpc_creg_check_mesg(struct client_vars *ce);
int rpc_creg_tabs_open(struct client_vars *ce);
int rpc_creg_new_tab(struct client_vars *ce);
int rpc_creg_tab_sel(struct client_vars *ce);
int rpc_creg_credit_tab_open(struct client_vars *ce);
int rpc_creg_update_tab_name(struct client_vars *ce);
int rpc_creg_update_conf_time(struct client_vars *ce);
int rpc_creg_config_del(struct client_vars *ce);
int rpc_creg_del_journal_item(struct client_vars *ce);

/* Container functions */
int rpc_creg_new_cont_item(struct client_vars *ce);
int rpc_creg_sel_cont_item(struct client_vars *ce);
int rpc_creg_new_cont_item_comp(struct client_vars *ce);
int rpc_creg_sel_cont_item_comp(struct client_vars *ce);
int rpc_creg_add_cont_item_comp(struct client_vars *ce);
int rpc_creg_sel_cont_item_inventory(struct client_vars *ce);
int rpc_creg_del_cont_item_comp(struct client_vars *ce);
int rpc_creg_upd_cont_item_comp(struct client_vars *ce);
int rpc_creg_upd_cont_name(struct client_vars *ce);

/* update functions */
int rpc_creg_upd_set_phase(struct client_vars *ce);
int rpc_creg_upd_download_start(struct client_vars *ce);
int rpc_creg_upd_download_status(struct client_vars *ce);
int rpc_creg_set_menu_option_item(struct client_vars *ce);

int rpc_creg_set_conf_item_image(struct client_vars *ce);
int rpc_cust_pos_display(struct client_vars *ce);
int rpc_creg_close_tab(struct client_vars *ce);

int rpc_creg_edit_option(struct client_vars *ce);
int rpc_creg_set_payment_col(struct client_vars *ce);

int rpc_creg_open_invt_adj_batch(struct client_vars *ce);
int rpc_creg_del_adj_batch_item(struct client_vars *ce);
int rpc_creg_post_batch_adjust(struct client_vars *ce);

int	rpc_creg_set_report_sort_parm(struct client_vars *ce);
int	rpc_creg_set_report_cal_parm(struct client_vars *ce);

int	rpc_creg_set_refund_date(struct client_vars *ce);
int	rpc_creg_set_refund_invoice_id(struct client_vars *ce);
int	rpc_creg_set_refund_journal_id(struct client_vars *ce);

int rpc_creg_dec_refund_item_qty(struct client_vars *ce);
int rpc_creg_inc_refund_item_qty(struct client_vars *ce);

int rpc_creg_post_refund(struct client_vars *ce);
int rpc_creg_new_printer(struct client_vars *ce);

int rpc_creg_menu_item_update(struct client_vars *ce);
int rpc_creg_set_batch_edit_item(struct client_vars *ce);
int rpc_creg_open_sub_menu(struct client_vars *ce);
int rpc_creg_rcpt_set_header_idx(struct client_vars *ce);
int rpc_creg_rcpt_set_format(struct client_vars *ce);

int rpc_creg_del_qid(struct client_vars *ce);
int rpc_creg_set_pid(struct client_vars *ce);
int rpc_creg_set_qid(struct client_vars *ce);

