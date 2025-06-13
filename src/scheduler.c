#include <stdio.h>
#include "utlist.h"
#include "utils.h"

#include "memory_controller.h"

extern long long int CYCLE_VAL;


int is_row_hit(int channel, int rank, int bank, long long int row) {
    return dram_state[channel][rank][bank].state == ROW_ACTIVE &&
           dram_state[channel][rank][bank].active_row == row;
}

void init_scheduler_vars()
{
	// initialize all scheduler variables here

	return;
}

// write queue high water mark; begin draining writes if write queue exceeds this value
#define HI_WM 80

// end write queue drain once write queue has this many writes in it
#define LO_WM 40

// 1 means we are in write-drain mode for that channel
int drain_writes[MAX_NUM_CHANNELS];

/* Each cycle it is possible to issue a valid command from the read or write queues
   OR
   a valid precharge command to any bank (issue_precharge_command())
   OR 
   a valid precharge_all bank command to a rank (issue_all_bank_precharge_command())
   OR
   a power_down command (issue_powerdown_command()), programmed either for fast or slow exit mode
   OR
   a refresh command (issue_refresh_command())
   OR
   a power_up command (issue_powerup_command())
   OR
   an activate to a specific row (issue_activate_command()).

   If a COL-RD or COL-WR is picked for issue, the scheduler also has the
   option to issue an auto-precharge in this cycle (issue_autoprecharge()).

   Before issuing a command it is important to check if it is issuable. For the RD/WR queue resident commands, checking the "command_issuable" flag is necessary. To check if the other commands (mentioned above) can be issued, it is important to check one of the following functions: is_precharge_allowed, is_all_bank_precharge_allowed, is_powerdown_fast_allowed, is_powerdown_slow_allowed, is_powerup_allowed, is_refresh_allowed, is_autoprecharge_allowed, is_activate_allowed.
   */

void schedule(int channel)
{
    #define AGE_THRESHOLD 10000

    request_t *rd_ptr = NULL;
    request_t *wr_ptr = NULL;
    request_t *best = NULL;
    long long int best_arrival = -1;

    // Decide whether to drain writes
    if (write_queue_length[channel] > HI_WM || read_queue_length[channel] == 0) {
        drain_writes[channel] = 1;
    } else if (write_queue_length[channel] <= LO_WM) {
        drain_writes[channel] = 0;
    }

    // Create two sets: row-hit and others
    if (!drain_writes[channel]) {
        // Prioritize row-hit reads or aged reads
        LL_FOREACH(read_queue_head[channel], rd_ptr) {
            if (rd_ptr->command_issuable &&
                (is_row_hit(channel, rd_ptr->dram_addr.rank,
                                     rd_ptr->dram_addr.bank,
                                     rd_ptr->dram_addr.row) ||
                 (CYCLE_VAL - rd_ptr->arrival_time > AGE_THRESHOLD))) {
                if (!best || rd_ptr->arrival_time < best_arrival) {
                    best = rd_ptr;
                    best_arrival = rd_ptr->arrival_time;
                }
            }
        }
        // If no row-hit or aged read, pick oldest issuable read
        if (!best) {
            LL_FOREACH(read_queue_head[channel], rd_ptr) {
                if (rd_ptr->command_issuable) {
                    if (!best || rd_ptr->arrival_time < best_arrival) {
                        best = rd_ptr;
                        best_arrival = rd_ptr->arrival_time;
                    }
                }
            }
        }
    } else {
        // Prioritize row-hit writes or aged writes
        LL_FOREACH(write_queue_head[channel], wr_ptr) {
            if (wr_ptr->command_issuable &&
                (is_row_hit(channel, wr_ptr->dram_addr.rank,
                                     wr_ptr->dram_addr.bank,
                                     wr_ptr->dram_addr.row) ||
                 (CYCLE_VAL - wr_ptr->arrival_time > AGE_THRESHOLD))) {
                if (!best || wr_ptr->arrival_time < best_arrival) {
                    best = wr_ptr;
                    best_arrival = wr_ptr->arrival_time;
                }
            }
        }
        // If no row-hit or aged write, pick oldest issuable write
        if (!best) {
            LL_FOREACH(write_queue_head[channel], wr_ptr) {
                if (wr_ptr->command_issuable) {
                    if (!best || wr_ptr->arrival_time < best_arrival) {
                        best = wr_ptr;
                        best_arrival = wr_ptr->arrival_time;
                    }
                }
            }
        }
    }

    if (best) {
        issue_request_command(best);
    }
    return;
}

void scheduler_stats()
{
  /* Nothing to print for now. */
}