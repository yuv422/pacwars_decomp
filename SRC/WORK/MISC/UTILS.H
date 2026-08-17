/*
 * UTILS.H
 *
 * Public prototypes exported by UTILS.C (functions that appear in
 * PACWARS.EXE's global symbol table). Reconstructed from PACWARS.TXT.
 */
#ifndef UTILS_H
#define UTILS_H

#include "PACWARS.H"

typedef enum {
	F_WARNING = 0,
	F_ERROR = 1,
	F_INFO = 2
} FORM_TYPE;

int prog_init(void);
int load_mvaenv(void);
ENV_STRUCT far * get_mvaenv(void);
VAR_STRUCT far * get_mvavar(void);
SET_STRUCT far * get_mvaset(void);
void rbox_1x1(int row, int col, int width, int height);
void rbox_2x2(int row, int col, int width, int height);
void rfbox(int row, int col, int width, int height);
void vline_1(int row, int col, int height);
void vline_2(int row, int col, int height);
void hline_1(int row, int col, int length);
void hline_2(int row, int col, int length);
void cls_screen(void);
void cls_viewport(void);
int set_queue_indic(int status);
void set_qdisp(int status, int row, int col, int font);
int set_config_indic(int status);
int set_mail_indic(int status);
int kb_event(unsigned int far * inkey, unsigned int far * ext);
int kbstatus(void);
void kb_flush(void);
void wait_kb(void);
int kb_press(void);
int read_key(void);
void push_kb(int key);
void set_func_key(unsigned int key, int status, void (*func_ptr)(void));
void set_utils_func(int status, void (*func_ptr1)(void), void (*func_ptr2)(void));
void func_key_process(int index);
void process_events(void);
void set_user_event(int secs, void (*event_ptr)(void));
void user_event(void);
void kb_pause(void);
void ch_disp_page(int page_num);
void set_mode(unsigned char mode);
int size_screen(int size);
void pos_cursor(int row, int col);
void rc_text(int row, int col, unsigned char far * text);
void rc_char(int row, int col, unsigned char pr_char, int repeat);
void rc_password(int row, int col, unsigned char far * pass_str);
void cls_line(int row, int col, int length);
void set_border(unsigned char colour);
void set_colour(unsigned char fore, unsigned char back);
int set_blink(int status);
int show_cursor(int status);
int get_cursor(int far * row, int far * col);
void far * save_screen(int row, int col, int width, int height, unsigned char far * sc_buffer);
void restore_screen(int row, int col, int width, int height, unsigned char far * sc_buffer);
void buffer_screen(unsigned int far * sc_ptr, unsigned char far * sc_buffer, int width, int height, int direction);
void far * alloc_screen_mem(int w, int h);
void ch_attrib(int row, int col, unsigned char fore, unsigned char back, int repeat);
void rbox_scroll(int row, int col, int width, int height, int direction, int num_lines);
int trim_spaces(unsigned char far * str_ptr);
void trim_newline(unsigned char far * str_ptr);
void clear_str(unsigned char far * text_str, int length);
void edit_str_init(void);
int edit_status(void);
int last_edit_status(void);
int edit_str_count(void);
int edit_toggle(void);
int get_next_edit(int exit_key, int left, int right, int up, int down, int enter, int next, int end);
void set_pgdn_status(int status);
void set_pgup_status(int status);
int get_pgdn_status(void);
int get_pgup_status(void);
int error_form(unsigned char far * title, unsigned char far * text_str, FORM_TYPE type);
int std_form(unsigned char far * title, unsigned char far * text_str, FORM_TYPE type, unsigned int valid_keys);
void form_bar(int row, int col, int width);
int colour_code(unsigned char far * code_text);
int centre_form_ln(unsigned char far * text_ptr, int text_col, int max_length);
void sys_err_form(int error_num, unsigned char far * title_str);
void log_error(int error_num);
void beep(unsigned int freq, int wait_length);
unsigned char single_input(int row, int col, unsigned char far * input_chars, unsigned char def_char);
void set_time_pos(int trow, int tcol, int drow, int dcol);
int show_time(int status);
void disp_time(void);
void disp_date(int wday, int day, int month, int year);
void top_message(unsigned char far * text_str1, unsigned char far * text_str2);
void clear_message(void);
int confirm_exit(unsigned char far * prompt);
void get_date(unsigned char far * date_str);
void get_time(unsigned char far * time_str);
void set_error_str(unsigned char far * text_str);
int get_error_str(unsigned char far * text_str);
void calc_error_str(unsigned char far * text_str);
int c_break(void);
void break_off(void);
void temp_message(int status, unsigned char far * mess_str);
void f1_bar(void);
void f1_query_bar(void);
void f2_edit_bar(int status);
void clear_bar(void);
void edit_error(void);
void edit_bar(void);
void pjdb_error(int fred);
void l_wait_init(void);
void l_wait_end(void);

#endif /* UTILS_H */
