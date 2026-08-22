/*
 * HOME.C
 *
 * home() -- computes an X/Y "homing direction" (-1, 0, +1) for a missile
 * chasing a target on a wraparound (maze-style) playfield, writing the
 * results through x_dir_flag/y_dir_flag. Used by the missile-steering
 * code to decide which way to nudge a homing missile each tick.
 *
 * Reconstructed from the raw disassembly at 2b1f:0002 (Ghidra's own
 * decompilation renders this fairly readably but mangles the return
 * value as CONCAT22(target_hoffset,in_AX) -- see the note on `ret`
 * below for what that artifact actually corresponds to).
 *
 * Both call sites (14df:15ee in an unnamed function and 14df:2884 in
 * init_bullet) discard the return value entirely -- the ADD SP that
 * cleans up the call's arguments is followed by code that reads other
 * locals, never AX -- so the exact value returned along some paths
 * (see below) has no observable effect on the game.
 */
#include "HOME.H"

int home(int missile_hoffset, int missile_voffset, int missx, int missy, int target_hoffset, int target_voffset, int targx, int targy, int far * x_dir_flag, int far * y_dir_flag, int max_hoffset, int max_voffset)
{
    int ret;

    *x_dir_flag = 0;
    *y_dir_flag = 0;

    if (missile_hoffset > target_hoffset) {
        /* Wrap-around case: missile at the max edge, target at 0 --
         * treat "backwards past the edge" as still moving forward (+1)
         * instead of the naive -1. Confirmed at 2b1f:0032-0043. */
        if (missile_hoffset == max_hoffset && target_hoffset == 0) {
            *x_dir_flag = 1;
        } else {
            *x_dir_flag = -1;
        }
        return 0;
    }

    if (missile_hoffset < target_hoffset) {
        /* Mirror wrap-around: missile at 0, target at the max edge.
         * Confirmed at 2b1f:0056-0069. */
        if (missile_hoffset == 0 && target_hoffset == max_hoffset) {
            *x_dir_flag = -1;
        } else {
            *x_dir_flag = 1;
        }
        return 0;
    }

    /* missile_hoffset == target_hoffset from here on. */

    if (missile_voffset == target_voffset) {
        if (missy < targy) {
            *y_dir_flag = 1;
        } else if (targy < missy) {
            *y_dir_flag = -1;
        } else {
            *y_dir_flag = 0;
        }

        /* The raw disassembly loads targx into AX here (2b1f:00b7) and
         * never reloads it before falling straight through to RETF at
         * the bottom of this branch (missile_voffset == target_voffset
         * always skips the two blocks below, since both re-test
         * missile_voffset vs target_voffset and find them equal) --
         * so this branch's real return value is always targx. */
        ret = targx;
        if (targx < missx) {
            *x_dir_flag = -1;
        } else if (missx < targx) {
            *x_dir_flag = 1;
        } else {
            *x_dir_flag = 0;
        }
        return ret;
    }

    if (missile_voffset < target_voffset) {
        if (missile_voffset == 0) {
            /* AX is set to target_voffset here (2b1f:00f3) *before* the
             * max_voffset wraparound test below, and nothing overwrites
             * it afterwards on either side of that test -- so ret ==
             * target_voffset whenever missile_voffset == 0, regardless
             * of which way the wraparound check goes. */
            ret = target_voffset;
            if (target_voffset == max_voffset) {
                *y_dir_flag = -1;
            } else {
                *y_dir_flag = 1;
            }
        } else {
            /* `ret` is never assigned on this path in the original --
             * whatever was left in AX from before this function's
             * missile_hoffset/missile_voffset comparisons (i.e. the
             * caller's own leftover AX, since nothing else in this
             * function touches AX before this point on this path) is
             * what gets returned. Left uninitialized here to match;
             * harmless since both call sites ignore the return value. */
            *y_dir_flag = 1;
        }
        return ret;
    }

    /* target_voffset < missile_voffset. Same "never assigned" situation
     * as the else-branch just above -- ret carries over whatever it
     * already held (uninitialized on this path). */
    if (missile_voffset == max_voffset && target_voffset == 0) {
        *y_dir_flag = 1;
    } else {
        *y_dir_flag = -1;
    }
    return ret;
}
