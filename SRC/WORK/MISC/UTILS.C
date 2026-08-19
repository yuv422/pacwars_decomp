/*
 * UTILS.C
 *
 * Reconstructed from PACWARS.EXE via the Borland TLINK debug symbol
 * dump (PACWARS.TXT). Function bodies are stubs pending full
 * decompilation; signatures (names, return types, parameters) are
 * taken from the debug info.
 */
#include "UTILS.H"

/*
 * Referenced from several keyboard-handling functions in this file
 * (kb_event, read_key, push_kb, kb_flush, wait_kb, kb_press) but not
 * declared anywhere in PACWARS.TXT's symbol tables -- unlike this
 * project's other recovered globals, these two have no real recorded
 * name, so the names below are inferred from their observed use in
 * read_key() rather than recovered.
 */
static char last_key_char;
static char key_ready;

/* storage for the __disp_page global declared extern in PACWARS.H */
int __disp_page;

int prog_init(void)
{
    return 0;
}

int load_mvaenv(void)
{
    return 0;
}

ENV_STRUCT far * get_mvaenv(void)
{
    return 0;
}

VAR_STRUCT far * get_mvavar(void)
{
    return 0;
}

SET_STRUCT far * get_mvaset(void)
{
    return 0;
}

void rbox_1x1(int row, int col, int width, int height)
{
}

void rbox_2x2(int row, int col, int width, int height)
{
}

void rfbox(int row, int col, int width, int height)
{
}

void vline_1(int row, int col, int height)
{
}

void vline_2(int row, int col, int height)
{
}

void hline_1(int row, int col, int length)
{
}

void hline_2(int row, int col, int length)
{
}

void cls_screen(void)
{
}

void cls_viewport(void)
{
}

int set_queue_indic(int status)
{
    return 0;
}

void set_qdisp(int status, int row, int col, int font)
{
}

int set_config_indic(int status)
{
    return 0;
}

int set_mail_indic(int status)
{
    return 0;
}

int kb_event(unsigned int far * inkey, unsigned int far * ext)
{
    return 0;
}

int kbstatus(void)
{
    return 0;
}

void kb_flush(void)
{
}

void wait_kb(void)
{
}

int kb_press(void)
{
    return 0;
}

int read_key(void)
{
    union REGS regs;
    int got_key;

    process_events();

    /* DOS INT 21h, AH=06h (direct console I/O), DL=FFh: poll for a
     * character without waiting. Returns with ZF set if none is ready,
     * clear (with the character in AL) if one is. */
    regs.h.ah = 0x06;
    regs.h.dl = 0xff;
    intdos(&regs, &regs);

    got_key = (regs.x.flags & 0x40) == 0;
    if (got_key) {
        last_key_char = regs.h.al;
        key_ready = 1;
    }
    return got_key;
}

void push_kb(int key)
{
}

void set_func_key(unsigned int key, int status, void (*func_ptr)(void))
{
}

void set_utils_func(int status, void (*func_ptr1)(void), void (*func_ptr2)(void))
{
}

void func_key_process(int index)
{
}

void process_events(void)
{
}

void set_user_event(int secs, void (*event_ptr)(void))
{
}

void user_event(void)
{
}

void kb_pause(void)
{
}

void ch_disp_page(int page_num)
{
}

void set_mode(unsigned char mode)
{
    union REGS regs;

    /* BIOS INT 10h, AH=00h: set video mode (AL = mode) */
    regs.h.ah = 0;
    regs.h.al = mode;
    int86(0x10, &regs, &regs);
}

int size_screen(int size)
{
    return 0;
}

void pos_cursor(int row, int col)
{
}

void rc_text(int row, int col, unsigned char far * text)
{
}

void rc_char(int row, int col, unsigned char pr_char, int repeat)
{
}

void rc_password(int row, int col, unsigned char far * pass_str)
{
}

void cls_line(int row, int col, int length)
{
}

void set_border(unsigned char colour)
{
}

void set_colour(unsigned char fore, unsigned char back)
{
}

int set_blink(int status)
{
    return 0;
}

int show_cursor(int status)
{
    return 0;
}

int get_cursor(int far * row, int far * col)
{
    return 0;
}

void far * save_screen(int row, int col, int width, int height, unsigned char far * sc_buffer)
{
    return 0;
}

void restore_screen(int row, int col, int width, int height, unsigned char far * sc_buffer)
{
}

void buffer_screen(unsigned int far * sc_ptr, unsigned char far * sc_buffer, int width, int height, int direction)
{
}

void far * alloc_screen_mem(int w, int h)
{
    return 0;
}

void ch_attrib(int row, int col, unsigned char fore, unsigned char back, int repeat)
{
}

void rbox_scroll(int row, int col, int width, int height, int direction, int num_lines)
{
}

int trim_spaces(unsigned char far * str_ptr)
{
    return 0;
}

void trim_newline(unsigned char far * str_ptr)
{
}

void clear_str(unsigned char far * text_str, int length)
{
}

void edit_str_init(void)
{
}

int edit_status(void)
{
    return 0;
}

int last_edit_status(void)
{
    return 0;
}

int edit_str_count(void)
{
    return 0;
}

int edit_toggle(void)
{
    return 0;
}

int get_next_edit(int exit_key, int left, int right, int up, int down, int enter, int next, int end)
{
    return 0;
}

void set_pgdn_status(int status)
{
}

void set_pgup_status(int status)
{
}

int get_pgdn_status(void)
{
    return 0;
}

int get_pgup_status(void)
{
    return 0;
}

int error_form(unsigned char far * title, unsigned char far * text_str, FORM_TYPE type)
{
    return 0;
}

int std_form(unsigned char far * title, unsigned char far * text_str, FORM_TYPE type, unsigned int valid_keys)
{
    return 0;
}

void form_bar(int row, int col, int width)
{
}

int colour_code(unsigned char far * code_text)
{
    return 0;
}

int centre_form_ln(unsigned char far * text_ptr, int text_col, int max_length)
{
    return 0;
}

void sys_err_form(int error_num, unsigned char far * title_str)
{
}

void log_error(int error_num)
{
}

void beep(unsigned int freq, int wait_length)
{
}

unsigned char single_input(int row, int col, unsigned char far * input_chars, unsigned char def_char)
{
    return 0;
}

void set_time_pos(int trow, int tcol, int drow, int dcol)
{
}

int show_time(int status)
{
    return 0;
}

void disp_time(void)
{
}

void disp_date(int wday, int day, int month, int year)
{
}

void top_message(unsigned char far * text_str1, unsigned char far * text_str2)
{
}

void clear_message(void)
{
}

int confirm_exit(unsigned char far * prompt)
{
    return 0;
}

void get_date(unsigned char far * date_str)
{
}

void get_time(unsigned char far * time_str)
{
}

void set_error_str(unsigned char far * text_str)
{
}

int get_error_str(unsigned char far * text_str)
{
    return 0;
}

void calc_error_str(unsigned char far * text_str)
{
}

int c_break(void)
{
    return 0;
}

void break_off(void)
{
}

void temp_message(int status, unsigned char far * mess_str)
{
}

void f1_bar(void)
{
}

void f1_query_bar(void)
{
}

void f2_edit_bar(int status)
{
}

void clear_bar(void)
{
}

void edit_error(void)
{
}

void edit_bar(void)
{
}

void pjdb_error(int fred)
{
}

void l_wait_init(void)
{
}

int fill_waitlog(WAITLOG_STRUCT far * waitlog)
{
    return 0;
}

void l_wait_end(void)
{
}

