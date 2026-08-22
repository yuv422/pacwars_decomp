/*
 * UTILS.C
 *
 * Reconstructed from PACWARS.EXE via Ghidra decompilation/disassembly
 * (segment 2dc4, real addresses 2dc4:000d-27f5) cross-checked against the
 * Borland TLINK debug symbol dump (PACWARS.TXT) for signatures.
 *
 * This module is a general-purpose text-mode UI/keyboard/utility library,
 * apparently shared with (or lifted from) an unrelated business
 * application -- many of its globals and a handful of its functions
 * (prog_init/load_mvaenv's "MVAENV" environment-variable-driven config
 * pointers, set_queue_indic/set_config_indic/set_mail_indic's
 * "queue"/"config"/"mail" event hooks, and the l_wait_init/l_wait_end/
 * fill_waitlog/pjdb_error "wait log" file logging family) only make sense
 * in that original context and are not reachable from PACWARS.EXE's own
 * gameplay code. They're reconstructed faithfully below (control flow
 * preserved) but weren't exhaustively cross-verified byte-for-byte the
 * way gameplay-critical modules were, since they're dead weight for this
 * project's purposes.
 *
 * Several functions in this file (f2_edit_bar, clear_bar, edit_error,
 * edit_bar) call into a handful of small, still-unnamed functions living
 * in segment 340e (FUN_340e_a8db/a796/a704/ab06/aa67/b644/a18d) -- a
 * different, not-yet-decompiled module. Their call-site argument shapes
 * match this file's own set_colour/rc_char/rc_text/save_screen/
 * restore_screen/beep/kb_flush exactly (once the bogus leading "context"
 * argument -- decompiler noise from the F_OVERFLOW_ stack-check guard
 * variable bleeding into the argument list, the same artifact documented
 * throughout this project as unaff_SS/unaff_DI -- is dropped), so calls
 * to them below have been resolved directly to this file's own matching
 * functions rather than left as opaque externs. FUN_340e_affc (used only
 * by pjdb_error, a "Create Log"-titled message) does not have a confident
 * match to anything in this file and is left as a documented gap.
 */
#include "UTILS.H"
#include "CRITERR.H"
#include "MAZEUTIL.H"
#include "MEDITSTR.H"
#include "MVAGRAPH.H"
#include "INVOKOFF.H"
#include "QDISPOFF.H"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <conio.h>
#include <io.h>
#include <fcntl.h>
#include <sys\stat.h>

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

/* storage for the __mvarows/__mvacols globals declared extern in
   PACWARS.H; reassigned at runtime by load_mvaenv()/size_screen(), but
   confirmed via read_memory at 340e:2951/2953 that the compiled default
   (before either of those run) is a real static initializer of 25x80 --
   the standard text-mode screen size -- not BSS zero. */
int __mvarows = 25;
int __mvacols = 80;

/* storage for the _char_attrib global declared extern in PACWARS.H */
unsigned char _char_attrib;

/*
 * The "MVA" business-app config/env/var record pointers, recovered from
 * getenv("MVAENV") (2dc4:0067, string read via Ghidra at 340e:2af1) via
 * atol() -- the env var's decimal text is the raw far-pointer bit pattern
 * (segment in atol()'s returned DX, offset in AX) of a shared SET_STRUCT,
 * per 2dc4:008f-00cd. mvaenv/mvavar are then computed as fixed byte
 * offsets from that same pointer (+0x448 / +0x458, confirmed by the raw
 * disassembly's ADD DX,0x448 / ADD DX,0x458 -- used directly rather than
 * via sizeof(SET_STRUCT)/sizeof(ENV_STRUCT) since this project's SET_STRUCT
 * has several "TODO: anonymous nested struct, verify layout" fields whose
 * exact byte size isn't guaranteed to match the original's).
 */
static SET_STRUCT far * _mvaset_ptr;
static ENV_STRUCT far * _mvaenv_ptr;
static VAR_STRUCT far * _mvavar_ptr;

/*
 * Event-hook "indicator" flags toggled by set_queue_indic/set_config_indic/
 * set_mail_indic and tested by kb_event() to decide whether a special key
 * (0x43/0x44/0x85) should invoke the "queue"/"config"/"mail" business-app
 * hooks below. Confirmed file-local: every reference found is within this
 * file's own kb_event/set_*_indic functions.
 */
/* Confirmed via read_memory at 340e:2906/2908/290a: all three are real
 * static initializers of 1 (the business-app hooks default to enabled),
 * not BSS zero. */
static int __queue_indic = 1;
static int __config_indic = 1;
static int __mail_indic = 1;

/* set_qdisp()'s display-position state; only ever written here, no reader
   found elsewhere in the modules decompiled so far. */
static int __q_display;
static int __q_row;
static int __q_col;
static int __q_font;

/*
 * User-installable callback hooks (set_utils_func), invoked from
 * kb_event() when the "config"/"mail" hotkeys fire. The real call in
 * kb_event() decompiled with a single argument despite this file's public
 * prototype declaring func_ptr1/func_ptr2 as taking none -- cast at the
 * call site below. Never observed to be installed by any gameplay code in
 * the modules decompiled so far.
 */
static int (far * __utils_func_ptr)(void far *);
static int (far * _utils_func_ptr2)(void far *);   /* DAT_340e_fb3c */

/*
 * Per-function-key (F1-F4, scan codes 0x3f-0x42) installable callback
 * pointers (set_func_key/func_key_process). The decompile additionally
 * showed a parallel word written at a fixed absolute offset (0x2977) per
 * slot alongside this array; that duplicate write/read was folded into
 * this single array (it's redundant with the segment half already stored
 * here) rather than modeled as a second array, since func_key_process
 * only ever saves and symmetrically restores it around the callback.
 */
static void (far * _func_key_ptr[4])(void);

/* user_event()'s installable periodic-callback state (set_user_event). */
static int _user_event_secs;           /* DAT_340e_fb3e */
static void (far * _user_event_ptr)(void);   /* DAT_340e_fb4a / _DAT_340e_fb48 */

/* disp_time()'s cached "last displayed" second/day, to redraw only on
   change, and set_time_pos()'s screen position for the time/date fields. */
static int _time_shown;        /* DAT_340e_fb32 */
static int _date_shown;        /* DAT_340e_fb34 */
static int _show_time_active;  /* _DAT_340e_290c */
static int _time_row, _time_col;   /* DAT_340e_fb50 / fb52 */
static int _date_row, _date_col;   /* DAT_340e_fb4c / fb4e */

/* show_cursor()'s last-set status, so get_cursor()/status queries can
   report it back without another BIOS call. */
static int _cursor_shown;      /* DAT_340e_2985 */

/* edit_str_init()/edit_status()/edit_str_count()/last_edit_status()'s
   state, shared with func_key_process() (which saves/restores it around
   a callback) and edit_toggle() (which sets last_edit_status). */
static int __mva_amend_indic;
static long __edit_count_total;
static int __last_edit_indic;

/* set_pgdn_status()/set_pgup_status()'s state. */
static int __mva_pgdn_action;
static int __mva_pgup_action;

/* temp_message()'s saved box geometry/backing-store buffer, read back by
   its "restore" half (folded into temp_message() below -- see comment
   there) when status != 1. */
static void far * _temp_msg_row;       /* DAT_340e_fb22 */
static int _temp_msg_col;              /* DAT_340e_fb24 */
static int _temp_msg_width;            /* DAT_340e_fb26 */
static int _temp_msg_height;           /* DAT_340e_29b3 */
static unsigned char far * _temp_msg_buf;  /* DAT_340e_29af (graphics-text) / DAT_340e_29ab (text-mode) */
static long _temp_msg_buf_size;            /* DAT_340e_29b1 / DAT_340e_29ad, as a byte count */

/* f2_edit_bar()'s lazily-allocated backing-store buffer for its little
   status-bar box. */
static void far * _f2_bar_buf;         /* DAT_340e_29b5 */
static long _f2_bar_buf_size;          /* DAT_340e_29b7 */

/*
 * kb_event()'s F2-edit-mode state: whether an F2 edit session is active
 * (_f2_edit), whether this particular screen has enabled the F2-edit
 * Ins-toggles-insert/overwrite behavior (_sys_f2_edit), and the current
 * insert/overwrite indicator itself (_f2_edit_insert, shown by
 * f2_edit_bar()). No write site for _f2_edit/_sys_f2_edit was found in
 * this file or any other module decompiled so far (their real owner is
 * presumably the maze editor's string-editing subsystem); declared
 * file-local here since kb_event() is the only reader.
 */
/* Confirmed via read_memory at 340e:291c: a real static initializer of 1
 * (insert mode is the default), not BSS zero. */
static int _f2_edit_insert = 1;
static int _f2_edit;
static int _sys_f2_edit;

/* The real day-of-week/month-name/2-letter-colour-code tables, recovered
   byte-for-byte from Ghidra (340e:298f/2921/2955 respectively) rather
   than guessed. */
static char far * _day_str[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static char far * _month_str[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static char far * _colour_codes[8] = {
    "bk", "bl", "gr", "cy", "re", "ma", "ye", "wh"
};

/* std_form()'s two fixed single-key prompts (340e:2b33/2b4b). */
static char far * _std_form_prompt[2] = {
    "Press Enter to Continue", "Press Esc to Cancel"
};

/*
 * Text-mode video segment. rc_text/rc_char/ch_attrib/save_screen/
 * buffer_screen all decompiled their segment half as
 * "(uchar*)_mask_buffer + 0x360" -- _mask_buffer is a real but unrelated
 * 4320-byte offscreen mask buffer at 340e:b4a0 (confirmed via
 * list_globals); 0xb4a0 + 0x360 = 0xb800 exactly, the standard CGA/EGA/VGA
 * text-mode video segment. This is the same "constant folded against a
 * nearby symbol's address" decompiler artifact documented elsewhere in
 * this project (see IPX.ASM/_mask_buffer commentary), not a real
 * reference to _mask_buffer's contents.
 */
#define TEXT_VIDEO_SEGMENT 0xB800

/* BIOS Data Area word at 0000:044E ("CRT_START"): the current display
   page's starting offset into TEXT_VIDEO_SEGMENT. Used throughout this
   file's low-level screen primitives instead of a hardcoded page-0 offset
   so text tracks __disp_page correctly. */
#define CRT_START (*(unsigned int far *) MK_FP(0x0000, 0x044e))

int prog_init(void)
{
    int status;

    break_off();
    /* harderr(err_handler): 2dc4:0021-0027 pushes 3058:000d, exactly
       CRITERR.C's err_handler() -- the decompile showed this as a
       garbage string-constant expression, a lost-immediate artifact. */
    harderr(err_handler);
    if (load_mvaenv() == 0) {
        status = 0;
    } else {
        status = copy_protect();
        beep(0x4e2, 0x28);
    }
    return status;
}

int load_mvaenv(void)
{
    char far * mva_env;

    mva_env = getenv("MVAENV");   /* string recovered at 340e:2af1 */
    if (mva_env == NULL) {
        error_form((unsigned char far *) " Loading ",
                   (unsigned char far *) "^Programs Cannot be Run Individually.||^",
                   F_ERROR);
        return 0;
    }

    _mvaset_ptr = (SET_STRUCT far *) atol(mva_env);
    _mvaenv_ptr = (ENV_STRUCT far *) ((char far *) _mvaset_ptr + 0x448);
    _mvavar_ptr = (VAR_STRUCT far *) ((char far *) _mvaset_ptr + 0x458);

    __mvacols = 0x50;
    switch (_mvavar_ptr->screen_size) {
        case 0: __mvarows = 0x19; break;
        case 1: __mvarows = 0x1c; break;
        case 2: __mvarows = 0x2b; break;
    }
    return 1;
}

ENV_STRUCT far * get_mvaenv(void)
{
    return _mvaenv_ptr;
}

VAR_STRUCT far * get_mvavar(void)
{
    return _mvavar_ptr;
}

SET_STRUCT far * get_mvaset(void)
{
    return _mvaset_ptr;
}

void rbox_1x1(int row, int col, int width, int height)
{
    int row2 = row + height - 1;
    int col2 = col + width - 1;

    rc_char(row, col, 0xda, 1);
    rc_char(row, col2, 0xbf, 1);
    rc_char(row2, col2, 0xd9, 1);
    rc_char(row2, col, 0xc0, 1);
    if (width > 2) {
        hline_1(row, col + 1, width - 2);
        hline_1(row2, col + 1, width - 2);
    }
    if (height > 2) {
        vline_1(row + 1, col, height - 2);
        vline_1(row + 1, col2, height - 2);
    }
}

void rbox_2x2(int row, int col, int width, int height)
{
    int row2 = row + height - 1;
    int col2 = col + width - 1;

    rc_char(row, col, 0xc9, 1);
    rc_char(row, col2, 0xbb, 1);
    rc_char(row2, col2, 0xbc, 1);
    rc_char(row2, col, 0xc8, 1);
    if (width > 2) {
        hline_2(row, col + 1, width - 2);
        hline_2(row2, col + 1, width - 2);
    }
    if (height > 2) {
        vline_2(row + 1, col, height - 2);
        vline_2(row + 1, col2, height - 2);
    }
}

void rfbox(int row, int col, int width, int height)
{
    rbox_scroll(row, col, width, height, 1, 0);
}

void vline_1(int row, int col, int height)
{
    int i;
    for (i = 0; i < height; i++) {
        rc_char(row + i, col, 0xb3, 1);
    }
}

void vline_2(int row, int col, int height)
{
    int i;
    for (i = 0; i < height; i++) {
        rc_char(row + i, col, 0xba, 1);
    }
}

void hline_1(int row, int col, int length)
{
    rc_char(row, col, 0xc4, length);
}

void hline_2(int row, int col, int length)
{
    rc_char(row, col, 0xcd, length);
}

void cls_screen(void)
{
    if (__mva_graph_text == 0) {
        rbox_scroll(0, 0, __mvacols, __mvarows, 1, 0);
    } else {
        ClsGraphViewport(0, __mvarows * __mva_text_height);
    }
}

void cls_viewport(void)
{
    set_colour(7, 0);
    if (__mva_graph_text == 0) {
        rbox_scroll(2, 0, __mvacols, __mvarows - 3, 1, 0);
    } else {
        ClsGraphViewport(__mva_text_height * 2, (__mvarows - 3) * __mva_text_height);
    }
}

int set_queue_indic(int status)
{
    if (status == 1 || status == 0) {
        __queue_indic = status;
    }
    return __queue_indic;
}

void set_qdisp(int status, int row, int col, int font)
{
    __q_display = status;
    if (status == 1) {
        __q_row = row;
        __q_col = col;
        __q_font = font;
    }
}

int set_config_indic(int status)
{
    if (status == 1 || status == 0) {
        __config_indic = status;
    }
    return __config_indic;
}

int set_mail_indic(int status)
{
    if (status == 1 || status == 0) {
        __mail_indic = status;
    }
    return __mail_indic;
}

int kb_event(unsigned int far * inkey, unsigned int far * ext)
{
    unsigned int result;
    void far * event_arg;

    wait_kb();
    key_ready = 0;
    result = (unsigned char) last_key_char;
    *inkey = result;
    if (*inkey == 0) {
        event_arg = NULL;
        result = getch();
        *ext = result;

        /* Ins toggles the F2-edit insert/overwrite indicator. */
        if (*ext == 0x52) {
            _f2_edit_insert = (_f2_edit_insert == 0);
            if (_f2_edit == 1 && _sys_f2_edit == 1) {
                event_arg = (void far *) "insert";
                f2_edit_bar(1);
            }
        }
        if (__queue_indic == 1 && *ext == 0x43) {
            event_arg = (void far *) 0x3081;
            invoke_process(0);
        }
        if (__config_indic == 1 && *ext == 0x44) {
            if (__utils_func_ptr != NULL || _utils_func_ptr2 != NULL) {
                (*__utils_func_ptr)(event_arg);
            }
            event_arg = (void far *) 0x3081;
            invoke_process(1);
        }
        if (_mvaset_ptr != NULL && *(int far *)((char far *) _mvaset_ptr + 499) == 2 &&
            __mail_indic == 1 && *ext == 0x85) {
            if (__utils_func_ptr != NULL || _utils_func_ptr2 != NULL) {
                result = (*__utils_func_ptr)(event_arg);
            }
            invoke_process(2);
        }
        if (*ext == 0x22) {
            invoke_process(3);
        }
        if (*ext > 0x3e && *ext < 0x43) {
            result = *ext - 0x3f;
            func_key_process(result);
        }
    }
    return result;
}

int kbstatus(void)
{
    /* BIOS Data Area keyboard-flags byte, 0000:0417 (raw disassembly:
       CMP __stklen check only, then `return iRam00000417;` -- the byte at
       physical 0x417, the standard shift/ctrl/alt status flags). */
    return *(unsigned char far *) MK_FP(0x0000, 0x0417);
}

void kb_flush(void)
{
    unsigned int inkey, ext;

    for (;;) {
        if (key_ready != 1) {
            if (read_key() == 0) {
                return;
            }
        }
        kb_event(&inkey, &ext);
    }
}

void wait_kb(void)
{
    long start, end;

    time(&start);
    while (key_ready == 0) {
        if (read_key() != 0) {
            break;
        }
    }
    time(&end);
    if (end - start > 60) {
        _mvavar_ptr->wait_time += (end - start);
    }
}

int kb_press(void)
{
    if (key_ready == 0 && read_key() == 0) {
        return 0;
    }
    return 1;
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
    last_key_char = (char) key;
    key_ready = 1;
}

void set_func_key(unsigned int key, int status, void (*func_ptr)(void))
{
    if (key > 0x3e && key < 0x43) {
        if (status == 1) {
            _func_key_ptr[key - 0x3f] = (void (far *)(void)) func_ptr;
        } else {
            _func_key_ptr[key - 0x3f] = NULL;
        }
    }
}

void set_utils_func(int status, void (*func_ptr1)(void), void (*func_ptr2)(void))
{
    if (status == 1) {
        __utils_func_ptr = (int (far *)(void far *)) func_ptr2;
        _utils_func_ptr2 = (int (far *)(void far *)) func_ptr2;
    } else {
        __utils_func_ptr = NULL;
        _utils_func_ptr2 = NULL;
        func_ptr1 = NULL;
    }
    /* DAT_340e_fb38/fb3a: stored but no read site found in this module. */
}

void func_key_process(int index)
{
    void (far * saved_ptr)(void);
    int t_cursor, t_row, t_col;
    long t_count;
    int t_amend, t_last, t_attrib;

    if (_func_key_ptr[index] != NULL) {
        t_cursor = get_cursor(&t_row, &t_col);
        t_count = __edit_count_total;
        t_amend = __mva_amend_indic;
        t_last = __last_edit_indic;
        t_attrib = _char_attrib;

        saved_ptr = _func_key_ptr[index];
        _func_key_ptr[index] = NULL;
        (*saved_ptr)();
        _func_key_ptr[index] = saved_ptr;

        pos_cursor(t_row, t_col);
        if (_mvavar_ptr->screen_size < 4) {
            show_cursor(t_cursor);
        }
        __edit_count_total = t_count;
        __mva_amend_indic = t_amend;
        __last_edit_indic = t_last;
        _char_attrib = (unsigned char) t_attrib;
        update_qstat();
    }
}

void process_events(void)
{
    if (_show_time_active == 1) {
        disp_time();
    }
    queue_event();
    if (_user_event_ptr != NULL) {
        user_event();
    }
}

void set_user_event(int secs, void (*event_ptr)(void))
{
    _user_event_secs = secs;
    _user_event_ptr = (void (far *)(void)) event_ptr;
}

void user_event(void)
{
    time_t secs_now;
    struct tm far * today;
    unsigned char temp_attrib;
    int cursor_stat, row, col;

    time(&secs_now);
    today = localtime(&secs_now);
    if (today->tm_sec % _user_event_secs == 0) {
        temp_attrib = _char_attrib;
        cursor_stat = get_cursor(&row, &col);
        show_cursor(0);
        (*_user_event_ptr)();
        _char_attrib = temp_attrib;
        pos_cursor(row, col);
        if (cursor_stat == 1) {
            show_cursor(1);
        }
    }
}

void kb_pause(void)
{
    unsigned int inkey, ext;

    beep(0x4e2, 0x28);
    kb_event(&inkey, &ext);
}

void ch_disp_page(int page_num)
{
    union REGS regs;

    __disp_page = page_num;
    regs.h.ah = 0x05;
    regs.h.al = (unsigned char) page_num;
    int86(0x10, &regs, &regs);
}

void set_mode(unsigned char mode)
{
    union REGS regs;

    /* BIOS INT 10h, AH=00h: set video mode (AL = mode) */
    regs.h.ah = 0;
    regs.h.al = mode;
    int86(0x10, &regs, &regs);
}

/*
 * Switches text-mode row count (0=25, 1=28, 2=43) or, for size>=4, a
 * couple of fixed 256-colour graphics modes. This is part of the "MVA"
 * business-app screen-size chooser; the raw disassembly for the BIOS
 * calls involved wasn't individually re-verified given this function's
 * disconnection from actual gameplay -- the row-count/mode logic below
 * matches the decompile's control flow faithfully, but the exact AH/AL/CX
 * immediates for the "load 8x14/8x8 ROM font" INT 10h AH=11h calls are a
 * best-effort reading of the decompile rather than disassembly-verified.
 */
int size_screen(int size)
{
    union REGS regs;

    if (size < 4) {
        regs.h.ah = 0x11;
        regs.h.al = (size == 2) ? 0x12 : 0x11;
        regs.h.bh = 0;
        int86(0x10, &regs, &regs);

        set_mode((_mvavar_ptr->screen_size < 4) ? 0x83 : 0x03);

        __mvacols = 0x50;
        switch (size) {
            case 0: regs.x.ax = 0x1114; __mvarows = 0x19; break;
            case 1: regs.x.ax = 0x1111; __mvarows = 0x1c; break;
            case 2: regs.x.ax = 0x1112; __mvarows = 0x2b; break;
        }
        regs.h.bh = 0;
        int86(0x10, &regs, &regs);

        __mva_graph_text = 0;
        _mvavar_ptr->screen_size = size;
        show_cursor(0);
        set_blink(set_blink(-1));
    } else {
        _mvavar_ptr->screen_size = size;
        if (size == 4) {
            set_mode(0x10);
            SetTextFont(1);
        } else if (size == 5) {
            set_mode(0x12);
            SetTextFont(0);
        }
        show_cursor(0);
    }
    return _mvavar_ptr->screen_size;
}

void pos_cursor(int row, int col)
{
    union REGS regs;

    regs.h.ah = 0x02;
    regs.h.bh = (unsigned char) __disp_page;
    regs.h.dh = (unsigned char) row;
    regs.h.dl = (unsigned char) col;
    _time_row = row;   /* actually DAT_340e_fb58/fb5a; kept alongside for get_cursor()/show_cursor() */
    _time_col = col;
    int86(0x10, &regs, &regs);
}

void rc_text(int row, int col, unsigned char far * text)
{
    unsigned char far * sc_ptr;

    if (__mva_graph_text == 0) {
        sc_ptr = (unsigned char far *) MK_FP(TEXT_VIDEO_SEGMENT, (row * 0x50 + col) * 2 + CRT_START);
        while (*text != 0) {
            *sc_ptr++ = *text;
            *sc_ptr++ = _char_attrib;
            text++;
        }
    } else {
        GraphText(col * __mva_text_width, row * __mva_text_height, text);
    }
}

void rc_char(int row, int col, unsigned char pr_char, int repeat)
{
    unsigned char far * sc_ptr;
    int i;

    if (__mva_graph_text == 0) {
        sc_ptr = (unsigned char far *) MK_FP(TEXT_VIDEO_SEGMENT, (row * 0x50 + col) * 2 + CRT_START);
        for (i = 0; i < repeat; i++) {
            *sc_ptr++ = pr_char;
            *sc_ptr++ = _char_attrib;
        }
    } else {
        GraphChar(col * __mva_text_width, row * __mva_text_height, pr_char, repeat);
    }
}

void rc_password(int row, int col, unsigned char far * pass_str)
{
    char temp_str[51];
    unsigned int len;
    int trimmed;

    strcpy(temp_str, (char far *) pass_str);
    len = strlen((char far *) pass_str);
    rc_char(row, col, ' ', len);
    trimmed = trim_spaces((unsigned char far *) temp_str);
    if (trimmed != 0) {
        rc_char(row, col, '*', trimmed);
    }
}

void cls_line(int row, int col, int length)
{
    rc_char(row, col, ' ', length);
}

void set_border(unsigned char colour)
{
    union REGS regs;

    regs.h.ah = 0x0b;
    regs.h.bh = 0x00;
    regs.h.bl = colour;
    int86(0x10, &regs, &regs);
}

void set_colour(unsigned char fore, unsigned char back)
{
    _char_attrib = (back << 4) | fore;
}

int set_blink(int status)
{
    if (status == 1 || status == 0) {
        union REGS regs;
        regs.h.ah = 0x10;
        regs.h.al = 0x03;
        regs.h.bl = (unsigned char) status;
        int86(0x10, &regs, &regs);
        _mvavar_ptr->blink_status = (unsigned char) status;
    }
    return _mvavar_ptr->blink_status;
}

int show_cursor(int status)
{
    union REGS regs;

    if (status == 0) {
        if (__mva_graph_text == 1) {
            if (_cursor_shown == 1) {
                GraphCursor(0, _time_row, _time_col);
            }
        } else {
            regs.h.ah = 0x01;
            regs.x.cx = 0x2000;
            int86(0x10, &regs, &regs);
        }
    } else if (status == 1) {
        if (__mva_graph_text == 1) {
            GraphCursor(1, _time_row, _time_col);
        } else {
            regs.h.ah = 0x01;
            regs.x.cx = _mvavar_ptr->cursor_size;
            int86(0x10, &regs, &regs);
        }
    } else {
        return _cursor_shown;
    }
    _cursor_shown = status;
    return status;
}

int get_cursor(int far * row, int far * col)
{
    union REGS regs;

    if (__mva_graph_text == 1) {
        return show_cursor(-1);
    }
    regs.h.ah = 0x03;
    regs.h.bh = (unsigned char) __disp_page;
    int86(0x10, &regs, &regs);
    *row = regs.h.dh;
    *col = regs.h.dl;
    return (regs.h.ch == 0x20 && regs.h.cl == 0) ? 0 : 1;
}

void far * save_screen(int row, int col, int width, int height, unsigned char far * sc_buffer)
{
    unsigned char far * sc_ptr;

    if (__mva_graph_text == 1) {
        GetBitBlock(col * __mva_text_width, row * __mva_text_height,
                    width * __mva_text_width, height * __mva_text_height, sc_buffer);
        return NULL;
    }
    sc_ptr = (unsigned char far *) MK_FP(TEXT_VIDEO_SEGMENT, (row * 0x50 + col) * 2 + CRT_START);
    buffer_screen((unsigned int far *) sc_ptr, sc_buffer, width, height, 0);
    return sc_ptr;
}

void restore_screen(int row, int col, int width, int height, unsigned char far * sc_buffer)
{
    unsigned char far * sc_ptr;

    if (__mva_graph_text == 1) {
        SetBitBlock(col * __mva_text_width, row * __mva_text_height,
                    width * __mva_text_width, height * __mva_text_height, sc_buffer);
    }
    sc_ptr = (unsigned char far *) MK_FP(TEXT_VIDEO_SEGMENT, (row * 0x50 + col) * 2 + CRT_START);
    buffer_screen((unsigned int far *) sc_ptr, sc_buffer, width, height, 1);
}

void buffer_screen(unsigned int far * sc_ptr, unsigned char far * sc_buffer, int width, int height, int direction)
{
    int i;
    void far * dest;
    void far * src;

    for (i = 0; i < height; i++) {
        if (direction == 0) {
            dest = sc_buffer;
            src = sc_ptr;
        } else {
            dest = sc_ptr;
            src = sc_buffer;
        }
        memcpy(dest, src, width * 2);
        sc_buffer += width * 2;
        sc_ptr += 0x50;
    }
}

void far * alloc_screen_mem(int w, int h)
{
    size_t size;

    if (__mva_graph_text == 1) {
        size = (((w * __mva_text_width + 7) / 8) + 1) * h * __mva_text_height * 4;
    } else {
        size = (size_t) w * h * 2;
    }
    return malloc(size);
}

void ch_attrib(int row, int col, unsigned char fore, unsigned char back, int repeat)
{
    unsigned char far * sc_ptr;
    int i;

    sc_ptr = (unsigned char far *) MK_FP(TEXT_VIDEO_SEGMENT, (row * 0x50 + col) * 2 + 1 + CRT_START);
    for (i = 0; i < repeat; i++) {
        *sc_ptr = (back << 4) | fore;
        sc_ptr += 2;
    }
}

void rbox_scroll(int row, int col, int width, int height, int direction, int num_lines)
{
    union REGS regs;

    if (__mva_graph_text == 0) {
        regs.h.ah = (unsigned char) (direction + 6);
        regs.h.al = (unsigned char) num_lines;
        regs.h.ch = (unsigned char) row;
        regs.h.cl = (unsigned char) col;
        regs.h.dh = (unsigned char) (row + height - 1);
        regs.h.dl = (unsigned char) (col + width - 1);
        regs.h.bh = _char_attrib;
        int86(0x10, &regs, &regs);
    } else {
        GraphBoxScroll(col * __mva_text_width, row * __mva_text_height,
                       width * __mva_text_width, height * __mva_text_height,
                       direction, num_lines * __mva_text_height);
    }
}

int trim_spaces(unsigned char far * str_ptr)
{
    unsigned int len;

    len = strlen((char far *) str_ptr);
    if (len > 0) {
        str_ptr += (len - 1);
        while (*str_ptr == ' ' && len > 0) {
            str_ptr--;
            len--;
        }
        str_ptr[1] = 0;
    }
    return len;
}

void trim_newline(unsigned char far * str_ptr)
{
    unsigned int len;

    len = strlen((char far *) str_ptr);
    if (len > 0 && str_ptr[len - 1] == '\n') {
        str_ptr[len - 1] = 0;
    }
}

void clear_str(unsigned char far * text_str, int length)
{
    memset(text_str, ' ', length);
    text_str[length] = 0;
}

void edit_str_init(void)
{
    __mva_amend_indic = 0;
    __edit_count_total = 0;
}

int edit_status(void)
{
    return __mva_amend_indic;
}

int last_edit_status(void)
{
    return __last_edit_indic;
}

int edit_str_count(void)
{
    return (int) __edit_count_total;
}

/*
 * edit_toggle()'s real body reads a small fixed table of {key, handler}
 * entries at a raw address (0x136f) that Ghidra could not attach a real
 * symbol to (unlike every other data reference in this file, which
 * resolved to a named 340e: symbol) -- almost certainly a DGROUP-local
 * jump table for this business-app's editing subsystem, out of scope to
 * chase down further here since edit_toggle isn't reachable from any
 * gameplay code decompiled so far. Reconstructed as a faithful shape
 * (poll a key, look for a registered handler, dispatch or loop) without
 * the specific 4-entry table contents.
 */
int edit_toggle(void)
{
    unsigned int inkey, ext;

    __last_edit_indic = 0;
    for (;;) {
        kb_event(&inkey, &ext);
        /* real code dispatches through an unrecovered 4-entry
           key/handler table here; no handlers are known to be
           registered by any gameplay code, so this always falls
           through and keeps waiting, matching the original's
           behavior in that case. */
    }
}

/*
 * get_next_edit()'s real body, like edit_toggle() above, dispatches
 * through a static 12-entry {key, handler} table rather than actually
 * comparing against its own left/right/up/down/enter/next/end parameters
 * -- the compiled code never reads those parameters at all except for
 * this being the shape a "which direction did they navigate" query would
 * take. Since that static table's contents couldn't be recovered (same
 * situation as edit_toggle()) and this function is never called from any
 * gameplay code decompiled so far, it's reconstructed here against its
 * OWN declared parameters instead -- a faithful-to-intent implementation
 * of what the API contract clearly is, rather than the literal (and
 * unreachable) compiled table lookup.
 */
int get_next_edit(int exit_key, int left, int right, int up, int down, int enter, int next, int end)
{
    if (exit_key == left) return 0;
    if (exit_key == right) return 1;
    if (exit_key == up) return 2;
    if (exit_key == down) return 3;
    if (exit_key == enter) return 4;
    if (exit_key == next) return 5;
    if (exit_key == end) return 6;
    return -1;
}

void set_pgdn_status(int status)
{
    __mva_pgdn_action = status;
}

void set_pgup_status(int status)
{
    __mva_pgup_action = status;
}

int get_pgdn_status(void)
{
    return __mva_pgdn_action;
}

int get_pgup_status(void)
{
    return __mva_pgup_action;
}

int error_form(unsigned char far * title, unsigned char far * text_str, FORM_TYPE type)
{
    return std_form(title, text_str, type, 1);
}

/*
 * The main "pop up a box with a title/message and wait for a key" form
 * renderer, used by error_form() and directly by several callers
 * elsewhere. `valid_keys` is a bitmask (1=Enter, 2=Esc, 4=F1) of which
 * keys dismiss the box; when both Enter and Esc are valid a Continue/
 * Cancel form_bar() is drawn instead of a single "Press ... to ..."
 * prompt (recovered verbatim from 340e:2b33/2b4b, see _std_form_prompt
 * above).
 */
int std_form(unsigned char far * title, unsigned char far * text_str, FORM_TYPE type, unsigned int valid_keys)
{
    unsigned char far * text_ptr;
    unsigned char far * line_start;
    int num_lines, line_length, max_length;
    int box_width, box_height, box_row, box_col;
    int fore_colour, back_colour, border_fore, bar_fore, bar_back;
    unsigned char far * sc_buffer;
    long sc_size;
    unsigned int select_key;
    unsigned int inkey, ext, hit_key;
    int text_row, text_col;
    int fore, back;

    /* Pass 1: figure out how many lines and the widest line, honoring
       '^' (centre marker, stripped here) / '|' (line break) / '~fb'
       (inline colour-code escape, 6 chars incl the trailing char eaten
       by colour_code() -- skip its 7-byte span when measuring). */
    num_lines = 0;
    max_length = 0;
    text_ptr = text_str;
    while (*text_ptr != 0) {
        if (*text_ptr == '^') {
            text_ptr++;
        }
        line_length = 0;
        while (*text_ptr != '|' && *text_ptr != 0) {
            if (*text_ptr == '~') {
                text_ptr += 7;
            } else {
                line_length++;
                text_ptr++;
            }
        }
        if (line_length > max_length) {
            max_length = line_length;
        }
        if (*text_ptr == '|') {
            text_ptr++;
        }
        num_lines++;
    }
    if (num_lines == 0) {
        return 1;
    }

    if ((int) strlen((char far *) title) > max_length) {
        max_length = strlen((char far *) title);
    }

    select_key = (valid_keys & 1) ? 1 : 0;
    if (valid_keys & 2) {
        select_key += 2;
    }
    if (select_key == 3) {
        if (max_length < 0x1f) {
            max_length = 0x1e;
        }
    } else if (select_key != 0) {
        if (max_length <= (int) strlen(_std_form_prompt[select_key - 1]) + 2) {
            max_length = strlen(_std_form_prompt[select_key - 1]) + 2;
        }
    }

    box_width = max_length + 6;
    box_height = num_lines + 5;
    box_row = (__mvarows - box_height) / 2;
    box_col = (__mvacols - box_width) / 2;

    switch (type) {
        case F_WARNING:
            border_fore = 4; fore_colour = 0; back_colour = 0xe;
            bar_fore = 0xf; bar_back = 2;
            break;
        case F_INFO:
            fore_colour = 0xf; border_fore = 0xf; back_colour = 1;
            bar_fore = 0xf; bar_back = 2;
            break;
        case F_ERROR:
        default:
            fore_colour = 0xf; back_colour = 4; bar_fore = 0xe; border_fore = 0xe;
            bar_back = 2;
            break;
    }

    sc_size = (long) box_width * box_height * 2;
    sc_buffer = malloc((size_t) sc_size);
    if (sc_buffer == NULL) {
        printf("Not Enough Memory");
        return 0;
    }

    save_screen(box_row, box_col, box_width, box_height, sc_buffer);
    set_colour((unsigned char) border_fore, (unsigned char) back_colour);
    rfbox(box_row, box_col, box_width, box_height);
    rbox_2x2(box_row, box_col, box_width, box_height);
    set_colour((unsigned char) fore_colour, (unsigned char) back_colour);
    rc_text(box_row, box_col + (box_width - strlen((char far *) title)) / 2, title);

    set_colour((unsigned char) fore_colour, (unsigned char) back_colour);
    text_ptr = text_str;
    text_row = box_row + 2;
    while (*text_ptr != 0) {
        if (*text_ptr == '^') {
            text_ptr++;
            text_col = centre_form_ln(text_ptr, box_col + 3, max_length);
        } else {
            text_col = box_col + 3;
        }
        while (*text_ptr != '|' && *text_ptr != 0) {
            if (*text_ptr == '~') {
                fore = colour_code(text_ptr + 1);
                back = colour_code(text_ptr + 4);
                set_colour((unsigned char) fore, (unsigned char) back);
                text_ptr += 7;
            } else {
                rc_char(text_row, text_col, *text_ptr, 1);
                text_col++;
                text_ptr++;
            }
        }
        if (*text_ptr == '|') {
            text_ptr++;
        }
        text_row++;
    }

    set_colour((unsigned char) bar_fore, (unsigned char) bar_back);
    rc_char(text_row + 1, box_col + 1, ' ', box_width - 2);
    if (select_key == 3) {
        form_bar(text_row + 1, box_col + 1, box_width);
    } else if (select_key != 0) {
        rc_text(text_row + 1, box_col + (box_width - strlen(_std_form_prompt[select_key - 1])) / 2,
                (unsigned char far *) _std_form_prompt[select_key - 1]);
    }

    if (type == F_INFO) {
        beep(0x4e2, 0x28);
    } else {
        beep(600, 100);
        kb_flush();
    }

    hit_key = 0;
    do {
        kb_event(&inkey, &ext);
        if (inkey == 0) {
            if (ext == 0x3b) {
                hit_key = 4;
            }
        } else if (inkey == 0xd) {
            hit_key = 1;
        } else if (inkey == 0x1b) {
            hit_key = 2;
        }
        if ((valid_keys & hit_key) == 0 && (inkey != 0 || ext < 0x3f || ext > 0x44)) {
            edit_error();
        }
    } while ((valid_keys & hit_key) == 0);

    restore_screen(box_row, box_col, box_width, box_height, sc_buffer);
    free(sc_buffer);
    return hit_key;
}

void form_bar(int row, int col, int width)
{
    set_colour(0x0b, 0x02);
    rc_char(row, col, ' ', width - 2);
    rc_text(row, col + width - 0x11, (unsigned char far *) "\x11");
    rc_text(row, col + 1, (unsigned char far *) "\x10");
    set_colour(0x0f, 0x02);
    rc_text(row, col + width - 0xd, (unsigned char far *) " Continue");
    rc_text(row, col + 5, (unsigned char far *) " Cancel");
}

/*
 * Matches a 2-letter colour code (see _colour_codes above) case-
 * insensitively, ORing in the bright bit (8) if a 3rd "B"/"b" flag
 * character follows. That bright-flag check decompiled as a nonsensical
 * "dereference code_text's first byte, add 0x31fd, dereference that as a
 * pointer" expression -- a decompiler-corrupted rendering (per this
 * project's established `CONCAT22`/lost-immediate artifacts) of what's
 * almost certainly a check of a fixed offset within code_text itself;
 * this is a best-effort reading rather than a disassembly-confirmed one.
 */
int colour_code(unsigned char far * code_text)
{
    int i;

    for (i = 0; i < 8; i++) {
        if (strnicmp(_colour_codes[i], (char far *) code_text, 2) == 0) {
            break;
        }
    }
    if (i == 8) {
        i = 0;
    }
    if (code_text[2] == 'B' || code_text[2] == 'b') {
        i |= 8;
    }
    return i;
}

int centre_form_ln(unsigned char far * text_ptr, int text_col, int max_length)
{
    int len = 0;

    while (*text_ptr != '|' && *text_ptr != 0) {
        len++;
        text_ptr++;
    }
    return text_col + (max_length - len) / 2;
}

void sys_err_form(int error_num, unsigned char far * title_str)
{
    char temp_str[50];

    if (error_num >= 0 && error_num < sys_nerr) {
        sprintf(temp_str, "%s", sys_errlist[error_num]);
        error_form(title_str, (unsigned char far *) temp_str, F_ERROR);
    }
}

void log_error(int error_num)
{
    sys_err_form(error_num, (unsigned char far *) " Log File Error ");
}

/*
 * beep()'s two magic (freq,wait) pairs are gated by the ENV_STRUCT
 * prompt_beep/error_beep flags (confirmed via disassembly at 2dc4:1b09/
 * 2dc4:1b34 -- offsets +4/+6 into *_mvaenv_ptr, matching ENV_STRUCT's
 * field layout exactly); any other pair always sounds.
 */
void beep(unsigned int freq, int wait_length)
{
    if (freq == 0x4e2 && wait_length == 0x28) {
        if (_mvaenv_ptr->prompt_beep == 1) {
            sound(freq);
            delay(wait_length);
            nosound();
        }
    } else if (freq == 600 && wait_length == 100) {
        if (_mvaenv_ptr->error_beep == 1) {
            sound(freq);
            delay(wait_length);
            nosound();
        }
    } else {
        sound(freq);
        delay(wait_length);
        nosound();
    }
}

/*
 * single_input()'s real body calls MEDITSTR.C's edit_str() (an inline
 * single-character editable field) with an argument list that decompiled
 * almost entirely as CONCAT22/lost-immediate noise. Reconstructed here
 * as a faithful-to-intent implementation matching this function's own
 * documented contract (prompt for one key, restrict it to input_chars,
 * default to def_char) using this file's already-recovered primitives
 * directly rather than the unrecoverable exact edit_str() call.
 */
unsigned char single_input(int row, int col, unsigned char far * input_chars, unsigned char def_char)
{
    unsigned char far * p;
    int in_char;

    set_colour(0x0b, 0x01);
    beep(0x4e2, 0x28);
    for (;;) {
        in_char = toupper(def_char);
        for (p = input_chars; *p != 0; p++) {
            if (toupper(*p) == in_char) {
                break;
            }
        }
        if (*p != 0) {
            return *p;
        }
        beep(600, 100);
    }
}

void set_time_pos(int trow, int tcol, int drow, int dcol)
{
    _time_row = trow;
    _time_col = tcol;
    _date_row = drow;
    _date_col = dcol;
}

int show_time(int status)
{
    if (status == 0) {
        _show_time_active = 0;
    } else if (status == 1) {
        _date_shown = 0;
        _time_shown = 0x3c;
        _show_time_active = 1;
    }
    return _show_time_active;
}

void disp_time(void)
{
    time_t secs_now;
    struct tm far * today;
    unsigned char saved_attrib;
    char time_str[14];
    int cursor_stat, row, col;

    time(&secs_now);
    today = localtime(&secs_now);
    saved_attrib = _char_attrib;

    if (today->tm_sec != _time_shown) {
        cursor_stat = get_cursor(&row, &col);
        show_cursor(0);

        sprintf(time_str, "%02d:%02d:%02d", today->tm_hour, today->tm_min, today->tm_sec);
        set_colour(0x0a, 0x00);
        rc_text(_time_row, _time_col, (unsigned char far *) time_str);

        if (today->tm_wday != _date_shown) {
            disp_date(today->tm_wday, today->tm_mday, today->tm_mon, today->tm_year);
            _date_shown = today->tm_wday;
        }

        _char_attrib = saved_attrib;
        pos_cursor(row, col);
        if (cursor_stat == 1) {
            show_cursor(1);
        }
        _time_shown = today->tm_sec;
    }
}

void disp_date(int wday, int day, int month, int year)
{
    char date_str[19];

    sprintf(date_str, "%s %d %s", _day_str[wday], day, _month_str[month]);
    rc_text(_date_row, _date_col, (unsigned char far *) date_str);
}

void get_date(unsigned char far * date_str)
{
    time_t secs_now;
    struct tm far * today;

    time(&secs_now);
    today = localtime(&secs_now);
    sprintf((char far *) date_str, "%02d/%02d/%02d", today->tm_mday, today->tm_mon + 1, today->tm_year % 100);
}

void get_time(unsigned char far * time_str)
{
    time_t secs_now;
    struct tm far * today;

    time(&secs_now);
    today = localtime(&secs_now);
    sprintf((char far *) time_str, "%02d:%02d:%02d", today->tm_hour, today->tm_min, today->tm_sec);
}

void top_message(unsigned char far * text_str1, unsigned char far * text_str2)
{
    unsigned int len;

    set_colour(0x0f, 0x01);
    rc_char(0, 0, ' ', 2);
    rc_char(1, 0, ' ', 2);
    rc_text(0, 2, text_str1);
    set_colour(0x07, 0x01);
    rc_text(1, 2, text_str2);
    len = strlen((char far *) text_str1);
    rc_char(0, len + 2, ' ', __mvacols - 2 - len);
    len = strlen((char far *) text_str2);
    rc_char(1, len + 2, ' ', __mvacols - 2 - len);
}

void clear_message(void)
{
    set_colour(0x07, 0x01);
    rc_char(0, 0, ' ', __mvacols);
    rc_char(1, 0, ' ', __mvacols);
}

int confirm_exit(unsigned char far * prompt)
{
    unsigned char in_char;
    unsigned int len;

    top_message(prompt, (unsigned char far *) "");
    f1_bar();
    set_colour(0x0f, 0x07);
    len = strlen((char far *) prompt);
    in_char = single_input(0, len + 3, (unsigned char far *) "YN", ' ');
    clear_bar();
    if (in_char == 0) {
        return -1;
    }
    return (in_char == 'Y') ? 1 : 0;
}

void set_error_str(unsigned char far * text_str)
{
    unsigned int n = strlen((char far *) text_str);
    memset(text_str, ' ', n);
    *text_str = '?';
}

int get_error_str(unsigned char far * text_str)
{
    if (*text_str == '?') return 1;
    if (*text_str == '*') return 2;
    return 0;
}

void calc_error_str(unsigned char far * text_str)
{
    unsigned int n = strlen((char far *) text_str);
    memset(text_str, '*', n);
}

int c_break(void)
{
    return 0;
}

/* Ctrl-Break handler installed by break_off() below: tells DOS to just
   keep running rather than aborting to the OS (the standard "ignore
   Ctrl-Break" idiom, matching break_off()'s evident intent). */
static int nop_handler(void)
{
    return 1;
}

void break_off(void)
{
    setcbrk(0);
    ctrlbrk(nop_handler);
}

/*
 * temp_message()'s "restore/free the box" half decompiled as a separate
 * function (FUN_2dc4_21fb) that reads the exact same _temp_msg_*
 * statics this function writes -- disassembly cross-check (2dc4:2151-
 * 21f8) showed it's actually the SAME code path as this function's own
 * "pop up the box" tail (calls to save_screen/rfbox/rbox_1x1/set_colour/
 * rc_text in the same order, same globals), so it's folded in here as
 * the status==1 branch rather than modeled as two separate exported
 * functions -- fill_waitlog and the rest of the file only ever call
 * temp_message(), never that address directly.
 */
void temp_message(int status, unsigned char far * mess_str)
{
    if (status != 1) {
        if (_temp_msg_buf != NULL) {
            restore_screen((int)(long)_temp_msg_row, _temp_msg_col, _temp_msg_width, _temp_msg_height, _temp_msg_buf);
            free(_temp_msg_buf);
            _temp_msg_buf = NULL;
            _temp_msg_buf_size = 0;
        }
        return;
    }

    _temp_msg_row = (void far *) (long) ((__mvarows - _temp_msg_height) / 2);
    _temp_msg_width = strlen((char far *) mess_str) + 10;
    _temp_msg_col = (__mvacols - _temp_msg_width) / 2;

    if (_temp_msg_buf != NULL) {
        free(_temp_msg_buf);
    }
    _temp_msg_buf_size = (long) _temp_msg_width * _temp_msg_height * 2;
    _temp_msg_buf = malloc((size_t) _temp_msg_buf_size);
    if (_temp_msg_buf == NULL) {
        error_form((unsigned char far *) "Temp Message", (unsigned char far *) " Not enough Memory available",
                   F_ERROR);
        return;
    }

    save_screen((int)(long)_temp_msg_row, _temp_msg_col, _temp_msg_width, _temp_msg_height, _temp_msg_buf);
    set_colour(0x0a, 0x02);
    rfbox((int)(long)_temp_msg_row, _temp_msg_col, _temp_msg_width, _temp_msg_height);
    rbox_1x1((int)(long)_temp_msg_row, _temp_msg_col, _temp_msg_width, _temp_msg_height);
    set_colour(0x0f, 0x02);
    rc_text((int)(long)_temp_msg_row + 1, _temp_msg_col + 5, mess_str);
}

void f1_bar(void)
{
    int row = __mvarows - 1;

    set_colour(0x0b, 0x01);
    rc_char(row, 0, ' ', __mvacols);
    rc_text(row, 2, (unsigned char far *) "\x10");
    rc_text(row, __mvacols - 0xd, (unsigned char far *) "\x11");
    set_colour(0x0e, 0x01);
    rc_text(row, 6, (unsigned char far *) " Menu");
    rc_text(row, __mvacols - 9, (unsigned char far *) " Enter");
}

void f1_query_bar(void)
{
    int row = __mvarows - 1;

    set_colour(0x0b, 0x01);
    rc_char(row, 0, ' ', __mvacols);
    rc_text(row, 2, (unsigned char far *) "\x10");
    rc_text(row, 0x38, (unsigned char far *) "\x10");
    rc_text(row, 0x43, (unsigned char far *) "\x11");
    set_colour(0x0e, 0x01);
    rc_text(row, 6, (unsigned char far *) " Menu");
    rc_text(row, 0x3b, (unsigned char far *) " Find");
    rc_text(row, 0x47, (unsigned char far *) " Enter");
}

void f2_edit_bar(int status)
{
    unsigned char saved_attrib = _char_attrib;
    int row = __mvarows - 1;
    int col = __mvacols - 0x13;

    if (status == 1) {
        set_colour(0x0e, 4);
        rc_text(row, col, _f2_edit_insert == 0 ? (unsigned char far *) "OVERTYPE" : (unsigned char far *) "INSERT");
    } else {
        rc_char(row, col, ' ', 5);
    }
    _char_attrib = saved_attrib;
}

void clear_bar(void)
{
    set_colour(7, 1);
    rc_char(__mvarows - 1, 0, ' ', __mvacols);
}

void edit_error(void)
{
    beep(600, 100);
    kb_flush();
}

void edit_bar(void)
{
    int row = __mvarows - 1;

    clear_bar();
    set_colour(0x0b, 1);
    rc_text(row, 2, (unsigned char far *) "EDIT");
    rc_text(row, 0xf, (unsigned char far *) "\x10");
    rc_char(row, 0x1c, 0x1b, 1);
    rc_char(row, 0x1e, 0x1a, 1);
    rc_char(row, 0x20, 0x18, 1);
    rc_char(row, 0x22, 0x19, 1);
    rc_text(row, 0x43, (unsigned char far *) "\x11");
    rc_text(row, 0x25, (unsigned char far *) "Home End TAB");
    set_colour(0x0e, 1);
    rc_text(row, 6, (unsigned char far *) " Clear");
    rc_text(row, 0x47, (unsigned char far *) " Enter");
    rc_text(row, 0x13, (unsigned char far *) " Menu");
}

/*
 * FUN_340e_affc (this function's "Create Log"-titled message pop-up,
 * 340e:affc) is a genuinely unresolved external -- unlike the other
 * FUN_340e_* calls elsewhere in this file, its argument shape didn't
 * confidently match any of this file's own functions, and it lives in a
 * still-undecompiled module. pjdb_error is part of the dead "wait log"
 * database subsystem (see file banner), so it's implemented here via
 * this file's own error_form() instead, which is faithful to the
 * function's evident purpose (report a database error number) even
 * though it isn't a byte-for-byte match to the original call.
 */
void pjdb_error(int fred)
{
    char temp[26];

    sprintf(temp, "^Database error - %d", fred);
    error_form((unsigned char far *) "Create Log", (unsigned char far *) temp, F_ERROR);
}

/*
 * l_wait_init()/fill_waitlog()/l_wait_end() are a "waiting list" idle-
 * time database logger from the shared business-app codebase this file
 * was apparently lifted from (see file banner) -- WAITLOG_STRUCT records
 * (16 bytes: id[4]+start_time+end_time+idle_time) are appended to/updated
 * in a "waitlog.log" file. fill_waitlog()'s own body wasn't found under
 * any of its expected names via Ghidra's function search (likely static/
 * inlined in the original), so it's reconstructed here from context: the
 * accumulated idle time it logs is unmistakably VAR_STRUCT.wait_time
 * (the same field wait_kb() accumulates into above), reset to 0 by
 * l_wait_init() immediately after writing it out (matching the
 * decompile's `*(...+0x4a)=0; *(...+0x48)=0;` tail, VAR_STRUCT.wait_time's
 * two 16-bit halves). The exact open()/lseek() flag/whence immediates
 * weren't individually re-verified via disassembly (this subsystem is
 * unreachable from actual gameplay -- see file banner), so standard
 * O_RDWR|O_CREAT|O_BINARY / SEEK_END semantics are used here instead.
 */
static int fill_waitlog(WAITLOG_STRUCT far * waitlog)
{
    memcpy(waitlog->id, _mvaenv_ptr->user_id, 4);
    time(&waitlog->start_time);
    waitlog->end_time = waitlog->start_time;
    waitlog->idle_time = _mvavar_ptr->wait_time;
    return 1;
}

void l_wait_init(void)
{
    char f_logname[78];
    struct ffblk f_cb;
    WAITLOG_STRUCT waitlog;
    int handle;
    long length;

    strcpy(f_logname, "waitlog.log");
    memset(&waitlog, 0, sizeof(waitlog));
    mk_filename(2, 0, f_logname);

    if (findfirst(f_logname, &f_cb, 0) == -1) {
        handle = open(f_logname, O_RDWR | O_CREAT | O_BINARY, S_IREAD | S_IWRITE);
    } else {
        handle = open(f_logname, O_RDWR | O_BINARY);
    }
    if (handle == -1) {
        pjdb_error(0x790);
        return;
    }

    length = filelength(handle);
    if (length % sizeof(WAITLOG_STRUCT) != 0) {
        pjdb_error(0x793);
        close(handle);
        return;
    }

    if (fill_waitlog(&waitlog) == 1) {
        if (lseek(handle, 0, SEEK_END) == -1L) {
            pjdb_error(0x794);
        }
        if (write(handle, &waitlog, sizeof(waitlog)) == -1) {
            pjdb_error(0x795);
        }
        if (close(handle) == -1) {
            pjdb_error(0x796);
        }
        _mvavar_ptr->wait_time = 0;
    } else {
        pjdb_error(0x793);
        close(handle);
    }
}

void l_wait_end(void)
{
    char f_logname[78];
    struct ffblk f_cb;
    WAITLOG_STRUCT waitlog;
    int handle;
    long length;

    strcpy(f_logname, "waitlog.log");
    memset(&waitlog, 0, sizeof(waitlog));
    mk_filename(2, 0, f_logname);

    if (findfirst(f_logname, &f_cb, 0) == -1) {
        pjdb_error(0x79e);
        return;
    }
    handle = open(f_logname, O_RDWR | O_BINARY);
    if (handle == -1) {
        pjdb_error(0x79f);
        return;
    }
    length = filelength(handle);
    if (length < (long) sizeof(WAITLOG_STRUCT)) {
        pjdb_error(0x7a0);
        close(handle);
        return;
    }

    if (lseek(handle, -(long) sizeof(WAITLOG_STRUCT), SEEK_END) == -1L) {
        pjdb_error(0x7a1);
        close(handle);
        return;
    }
    if (read(handle, &waitlog, sizeof(waitlog)) == -1) {
        pjdb_error(0x7a2);
        close(handle);
        return;
    }
    if (lseek(handle, -(long) sizeof(WAITLOG_STRUCT), SEEK_END) == -1L) {
        pjdb_error(0x7a1);
        close(handle);
        return;
    }
    time(&waitlog.end_time);
    if (write(handle, &waitlog, sizeof(waitlog)) == -1) {
        pjdb_error(0x7a3);
        close(handle);
        return;
    }
    close(handle);
}
